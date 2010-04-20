/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "Inventory.h"
#include "CraftingTool.h"
#include "PlayerObject.h"
#include "ResourceContainer.h"
#include "WorldManager.h"
#include "MessageLib/MessageLib.h"
#include "DatabaseManager/Database.h"
#include "Utils/utils.h"

#include <cassert>

//=============================================================================

Inventory::Inventory() : TangibleObject()
{
	mTanGroup = TanGroup_Inventory;
	mObjectLoadCounter = 1000;
}

//=============================================================================
//
// just removes all objects, dont need to send destroys to the client, since it always should be erased along with a parent
//
Inventory::~Inventory()
{
}

//=============================================================================


//============================================================================
// checks whether we have enough space in the inventory to accomodate
// amount items
bool Inventory::checkSlots(uint8 amount)
{
	// please note that just counting the amount of items is bound to be faulty as certain items
	// can (in theory) occupy more than one slot
	// we should iterate the list and count each items volume!
	// please note too, that Im not certain of the clients involvement of this
	// at this point the inventories max capacity is stored in the db table inventory_types
	if((mMaxSlots - getObjects()->size()) >= amount)
		return true;

	gLogger->logMsgF("Inventory::checkslots(): Inventory full : max Inv capacity :%u, current capacity %u, nr of items we tried to add", MSG_NORMAL, mMaxSlots,getObjects()->size(),amount);
	return false;
}

//=============================================================================

bool Inventory::updateCredits(int32 amount)
{
	if(mCredits + amount < 0)
		return(false);

	mCredits += amount;

	if(mParent->getType() == ObjType_Player)
		gMessageLib->sendInventoryCreditsUpdate(dynamic_cast<PlayerObject*>(mParent));

	gWorldManager->getDatabase()->ExecuteSqlAsync(NULL,NULL,"UPDATE inventories set credits=credits+%i WHERE id=%"PRIu64"",amount,mId);

	return(true);
}

//=============================================================================

void Inventory::handleObjectReady(Object* object,DispatchClient* client)
{
	TangibleObject* tangibleObject = dynamic_cast<TangibleObject*>(object);
	if(!tangibleObject)
	{
		gLogger->logMsgF("Inventory::handleObjectReady : Not a tangible ???", MSG_NORMAL);
		assert(false && "Inventory::handleObjectReady object is not tangible");
		return;
	}
	
	// reminder: objects are owned by the global map, inventory only keeps references

	//generally we presume that objects are created UNEQUIPPED
	//equipped objects are handled through the playerfactory on load
	gWorldManager->addObject(object,true);//true means its not added to the si!!

	// send the creates, if we are owned by a player
	if(PlayerObject* player = dynamic_cast<PlayerObject*>(mParent))
	{
		addObject(object,player);
	}

	else
		addObjectSecure(object);
	
}

//=============================================================================
//
// Build a list with uninsured items. (in inventory or equipped, that are un-insured, but can be insured)
//
// The list is sorted by the "@item_n:survey_tool_wind" names,
// making item of same "family" be grouped toghter.
//

void Inventory::getUninsuredItems(SortedInventoryItemList* insuranceList)
{
	// Clear the insurance list.
	insuranceList->clear();

	ObjectIDList::iterator invObjectIt = getObjects()->begin();

	// Items inside inventory and child objects.
	while (invObjectIt != getObjects()->end())
	{
		Object* object = gWorldManager->getObjectById((*invObjectIt));
		if (object&&object->hasInternalAttribute("insured"))
		{
			// gLogger->logMsgF("Inventory::insuranceListCreate: Found item with insurance attribute inside the inventory: %"PRIu64"", MSG_NORMAL,object->getId());
			if (!object->getInternalAttribute<bool>("insured"))
			{
				// Add the item to the insurance list.
				// gLogger->logMsgF("Inventory::insuranceListCreate: Found an uninsured item inside Inventory: %"PRIu64"", MSG_NORMAL,object->getId());

				// Handle the list.
				if (object->hasAttribute("original_name"))
				{
					SortedInventoryItemList::iterator it = insuranceList->begin();
					string itemName((int8*)object->getAttribute<std::string>("original_name").c_str());
					for (uint32 index = 0; index < insuranceList->size(); index++)
					{
						if (Anh_Utils::cmpistr(itemName.getAnsi(), (*it).first.getAnsi()) < 0)
						{
							break;
						}
						it++;
					}
					insuranceList->insert(it, std::make_pair(itemName,object->getId()));
				}
			}
		}
		invObjectIt++;
	}

	// Items equipped by the player.
	PlayerObject* player = dynamic_cast<PlayerObject*>(gWorldManager->getObjectById(this->getParentId()));
	if(!player)
		return;

	ObjectList* objList = player->getEquipManager()->getEquippedObjects();

	ObjectList::iterator equippedObjectIt = objList->begin();

	while (equippedObjectIt != objList->end())
	{
		Object* object = (*equippedObjectIt);
		if (object&&object->hasInternalAttribute("insured"))
		{
			// gLogger->logMsgF("Inventory::insuranceListCreate: Found equipped item with insurance attribute: %"PRIu64"", MSG_NORMAL,object->getId());
			if (!object->getInternalAttribute<bool>("insured"))
			{
				// Add the item to the insurance list.
				// gLogger->logMsgF("Inventory::insuranceListCreate: Found an uninsured equipped item: %"PRIu64"", MSG_NORMAL,object->getId());

				// Handle the list.
				if (object->hasAttribute("original_name"))
				{
					SortedInventoryItemList::iterator it = insuranceList->begin();
					string itemName((int8*)object->getAttribute<std::string>("original_name").c_str());
					for (uint32 index = 0; index < insuranceList->size(); index++)
					{
						if (Anh_Utils::cmpistr(itemName.getAnsi(), (*it).first.getAnsi()) < 0)
						{
							break;
						}
						it++;
					}
					insuranceList->insert(it, std::make_pair(itemName,object->getId()));
				}
			}
		}
		equippedObjectIt++;
	}
	delete objList;
}

//=============================================================================
//
// Build a list with insured items. (in inventory or equipped)
//
// The list is sorted by the "@item_n:survey_tool_wind" names,
// making item of same "family" be grouped toghter.
//

void Inventory::getInsuredItems(SortedInventoryItemList* insuranceList)
{
	// Clear the insurance list.
	insuranceList->clear();

	ObjectIDList::iterator invObjectIt = getObjects()->begin();

	// Items inside inventory and child objects.
	while (invObjectIt != getObjects()->end())
	{
		Object* object = gWorldManager->getObjectById((*invObjectIt));
		if (object&&object->hasInternalAttribute("insured"))
		{
			// gLogger->logMsgF("Inventory::insuranceListCreate: Found item with insurance attribute inside the inventory: %"PRIu64"", MSG_NORMAL,object->getId());
			if (object->getInternalAttribute<bool>("insured"))
			{
				// Add the item to the insurance list.
				// gLogger->logMsgF("Inventory::insuranceListCreate: Found an insured item inside Inventory: %"PRIu64"", MSG_NORMAL,object->getId());

				// Handle the list.
				if (object->hasAttribute("original_name"))
				{
					SortedInventoryItemList::iterator it = insuranceList->begin();
					string itemName((int8*)object->getAttribute<std::string>("original_name").c_str());
					for (uint32 index = 0; index < insuranceList->size(); index++)
					{
						if (Anh_Utils::cmpistr(itemName.getAnsi(), (*it).first.getAnsi()) < 0)
						{
							break;
						}
						it++;
					}
					insuranceList->insert(it, std::make_pair(itemName,object->getId()));
				}
			}
		}
		invObjectIt++;
	}

	// Items equipped by the player.
	PlayerObject* player = dynamic_cast<PlayerObject*>(gWorldManager->getObjectById(this->getParentId()));
	if(!player)
		return;

	ObjectList* objList = player->getEquipManager()->getEquippedObjects();
	ObjectList::iterator equippedObjectIt = objList->begin();

	while (equippedObjectIt != objList->end())
	{
		Object* object = (*equippedObjectIt);
		if (object->hasInternalAttribute("insured"))
		{
			// gLogger->logMsgF("Inventory::insuranceListCreate: Found equipped item with insurance attribute: %"PRIu64"", MSG_NORMAL,object->getId());
			if (object->getInternalAttribute<bool>("insured"))
			{
				// Add the item to the insurance list.
				// gLogger->logMsgF("Inventory::insuranceListCreate: Found an insured equipped item: %"PRIu64"", MSG_NORMAL,object->getId());

				// Handle the list.
				if (object->hasAttribute("original_name"))
				{
					SortedInventoryItemList::iterator it = insuranceList->begin();
					string itemName((int8*)object->getAttribute<std::string>("original_name").c_str());
					for (uint32 index = 0; index < insuranceList->size(); index++)
					{
						if (Anh_Utils::cmpistr(itemName.getAnsi(), (*it).first.getAnsi()) < 0)
						{
							break;
						}
						it++;
					}
					insuranceList->insert(it, std::make_pair(itemName,object->getId()));
				}
			}
		}
		equippedObjectIt++;
	}
	delete objList;
}

//=============================================================================
bool Inventory::itemExist(uint32 familyId, uint32 typeId)
{
	bool found = false;
	ObjectIDList::iterator invObjectIt = getObjects()->begin();

	// Items inside inventory and child objects.
	while (invObjectIt != getObjects()->end())
	{
		Object* object = getObjectById(*invObjectIt);
		Item* item = dynamic_cast<Item*>(object);
		if (item)
		{
			if ((item->getItemFamily() == familyId) && (item->getItemType() == typeId))
			{
				found = true;
				break;
			}
		}
		invObjectIt++;
	}

	if (!found)
	{
		// Items equipped by the player.
		PlayerObject* player = dynamic_cast<PlayerObject*>(gWorldManager->getObjectById(this->getParentId()));
		if(!player)
			return found;

		ObjectList* objList = player->getEquipManager()->getEquippedObjects();
		ObjectList::iterator equippedObjectIt = objList->begin();

		while (equippedObjectIt != objList->end())
		{
			Object* object = (*equippedObjectIt);
			Item* item = dynamic_cast<Item*>(object);
			if (item)
			{
				if ((item->getItemFamily() == familyId) && (item->getItemType() == typeId))
				{
					found = true;
					break;
				}
			}
			equippedObjectIt++;
		}
		delete objList;
	}
	return found;
}

//=============================================================================
//check for the containers capacity and return a fitting error message if necessary
//

bool Inventory::checkCapacity(uint8 amount, PlayerObject* player, bool sendMsg)
{
	if(player&&(getCapacity() - getHeadCount() < amount))
	{
		if(sendMsg)
			gMessageLib->sendSystemMessage(player,L"","error_message","inv_full");
		return false;
	}

	return true;
}