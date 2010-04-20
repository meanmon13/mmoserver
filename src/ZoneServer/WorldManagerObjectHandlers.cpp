/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "WorldManager.h"
#include "AdminManager.h"
#include "Buff.h"
#include "BuffEvent.h"
#include "BuffManager.h"
#include "BuildingObject.h"
#include "CellObject.h"
#include "HouseObject.h"
#include "PlayerObject.h"
#include "CharacterLoginHandler.h"
#include "Container.h"
#include "ConversationManager.h"
#include "CraftingSessionFactory.h"
#include "CraftingTool.h"
#include "CreatureSpawnRegion.h"
#include "GroupManager.h"
#include "GroupObject.h"
#include "Heightmap.h"
#include "MissionManager.h"
#include "MountObject.h"
#include "NpcManager.h"
#include "NPCObject.h"
#include "PlayerStructure.h"
#include "ResourceCollectionManager.h"
#include "ResourceManager.h"
#include "SchematicManager.h"
#include "TreasuryManager.h"
#include "Terminal.h"
#include "WorldConfig.h"
#include "ZoneOpcodes.h"
#include "ZoneServer.h"
#include "ZoneTree.h"
#include "HarvesterFactory.h"
#include "HarvesterObject.h"
#include "FactoryFactory.h"
#include "FactoryObject.h"
#include "Inventory.h"
#include "MissionObject.h"
#include "ObjectFactory.h"
#include "QuadTree.h"
#include "Shuttle.h"
#include "TicketCollector.h"
#include "ConfigManager/ConfigManager.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DataBinding.h"
#include "DatabaseManager/DatabaseResult.h"
#include "MessageLib/MessageLib.h"
#include "ScriptEngine/ScriptEngine.h"
#include "ScriptEngine/ScriptSupport.h"
#include "Utils/Scheduler.h"
#include "Utils/VariableTimeScheduler.h"
#include "Utils/utils.h"
#include "Common/MessageFactory.h"

#include <cassert>

//======================================================================================================================
//
// returns the id of the first object that has a private owner that match the requested one.
//
// This function is not used yet.
uint64 WorldManager::getObjectOwnedBy(uint64 theOwner)
{
	gLogger->logMsgF("WorldManager::getObjectOwnedBy: Invoked",MSG_NORMAL);
	ObjectMap::iterator it = mObjectMap.begin();
	uint64 ownerId = 0;

	while (it != mObjectMap.end())
	{
		if ( ((*it).second)->getPrivateOwner() == theOwner)
		{
			ownerId = (*it).first;
			gLogger->logMsgF("WorldManager::getObjectOwnedBy: Found an object with id = %"PRIu64"",MSG_NORMAL, ownerId);
			break;
		}
		it++;
	}
	return ownerId;
}


//======================================================================================================================
//
// adds an object to the world->to cells and the SI only, use manual if adding to containers, or cells on preload
//

bool WorldManager::addObject(Object* object,bool manual)
{
	uint64 key = object->getId();

	//make sure objects arnt added several times!!!!
	if(getObjectById(key))
	{
		gLogger->logMsgF("WorldManager::addObject Object already existant added several times or ID messup ???",MSG_HIGH);
		return false;
	}

	mObjectMap.insert(key,object);

	// if we want to set the parent manually or the object is from the snapshots and not a building, return
	if(manual)
	{
		return true;
	}

#if defined(_MSC_VER)
	if(object->getId() < 0x0000000100000000 && object->getType() != ObjType_Building)
#else
	if(object->getId() < 0x0000000100000000LLU && object->getType() != ObjType_Building)
#endif
	{
		// check if a crafting station - in that case add
		Item* item = dynamic_cast<Item*> (object);

		if(item)
		{
			if(!(item->getItemFamily() == ItemFamily_CraftingStations))
				return true;
		}
		else
		{
			return true;
		}
	}

	switch(object->getType())
	{
		// player, when a player enters a planet
		case ObjType_Player:
		{

			PlayerObject* player = dynamic_cast<PlayerObject*>(object);
			gLogger->logMsgF("New Player: %"PRIu64", Total Players on zone : %i",MSG_NORMAL,player->getId(),(getPlayerAccMap())->size() + 1);
			// insert into the player map
			mPlayerAccMap.insert(std::make_pair(player->getAccountId(),player));

			// insert into cell
			if(player->getParentId())
			{
				player->setSubZoneId(0);

				if(CellObject* cell = dynamic_cast<CellObject*>(getObjectById(player->getParentId())))
				{
					cell->addObjectSecure(player);
				}
				else
				{
					gLogger->logMsgF("WorldManager::addObject: couldn't find cell %"PRIu64"",MSG_HIGH,player->getParentId());
				}
			}
			// query the rtree for the qt region we are in
			else
			{
				if(QTRegion* region = mSpatialIndex->getQTRegion(player->mPosition.x,player->mPosition.z))
				{
					player->setSubZoneId((uint32)region->getId());
					region->mTree->addObject(player);
				}
				else
				{
					// we should never get here !
					gLogger->logMsg("WorldManager::addObject: could not find zone region in map");
					return false;
				}
			}

			// initialize
			initObjectsInRange(player);
			gMessageLib->sendCreatePlayer(player,player);

			// add ham to regeneration scheduler
			player->getHam()->updateRegenRates();	// ERU: Note sure if this is needed here.
			player->getHam()->checkForRegen();

			// onPlayerEntered event, notify scripts
			string params;
			params.setLength(sprintf(params.getAnsi(),"%s %s %u",getPlanetNameThis(),player->getFirstName().getAnsi(),static_cast<uint32>(mPlayerAccMap.size())));

			mWorldScriptsListener.handleScriptEvent("onPlayerEntered",params);

			// Start player world position update. Used when player don't get any events from client (player not moving).
			// addPlayerMovementUpdateTime(player, 1000);
		}
		break;

		case ObjType_Structure:
		{
		//	HarvesterObject* harvester = dynamic_cast<HarvesterObject*>(object);
			mStructureList.push_back(object->getId());
			mSpatialIndex->InsertPoint(key,object->mPosition.x,object->mPosition.z);

		}
		break;

		case ObjType_Building:
		{
			mStructureList.push_back(object->getId());
			BuildingObject* building = dynamic_cast<BuildingObject*>(object);
			
			mSpatialIndex->InsertRegion(key,building->mPosition.x,building->mPosition.z,building->getWidth(),building->getHeight());
		}
		break;


		case ObjType_Tangible:
		{
			uint64 parentId = object->getParentId();

			if(parentId == 0)
			{
				mSpatialIndex->InsertPoint(key,object->mPosition.x,object->mPosition.z);
			}
			else
			{
				CellObject* cell = dynamic_cast<CellObject*>(getObjectById(parentId));

				if(cell)
					cell->addObjectSecure(object);
				else
					gLogger->logMsgF("WorldManager::addObject couldn't find cell %"PRIu64"",MSG_NORMAL,parentId);
			}
		}
		break;

		// TODO: add moving creatures to qtregions
		case ObjType_NPC:
		case ObjType_Creature:
		case ObjType_Lair:
		{
			CreatureObject* creature = dynamic_cast<CreatureObject*>(object);

			if(creature->getCreoGroup() == CreoGroup_Shuttle)
				mShuttleList.push_back(dynamic_cast<Shuttle*>(creature));

			uint64 parentId = creature->getParentId();

			if(parentId)
			{
				CellObject* cell = dynamic_cast<CellObject*>(getObjectById(parentId));

				if(cell)
					cell->addObjectSecure(creature);
				else
					gLogger->logMsgF("WorldManager::addObject: couldn't find cell %"PRIu64"",MSG_HIGH,parentId);
			}
			else
			{

				switch(creature->getCreoGroup())
				{
					// moving creature, add to QT
					case CreoGroup_Vehicle :
					{
						if(QTRegion* region = mSpatialIndex->getQTRegion(creature->mPosition.x,creature->mPosition.z))
						{
							creature->setSubZoneId((uint32)region->getId());
							region->mTree->addObject(creature);
						}
						else
						{
							gLogger->logMsg("WorldManager::addObject: could not find zone region in map for creature");
							return false;
						}

					}
					break;

					// still creature, add to SI
					default :
					{
						mSpatialIndex->InsertPoint(key,creature->mPosition.x,creature->mPosition.z);
					}
				}


			}
		}
		break;

		case ObjType_Region:
		{
			RegionObject* region = dynamic_cast<RegionObject*>(object);

			mRegionMap.insert(std::make_pair(key,region));

			mSpatialIndex->InsertRegion(key,region->mPosition.x,region->mPosition.z,region->getWidth(),region->getHeight());

			if(region->getActive())
				addActiveRegion(region);
		}
		break;

		case ObjType_Intangible:
		{
			gLogger->logMsgF("Object of type ObjType_Intangible UNHANDLED in WorldManager::addObject:",MSG_HIGH);
		}
		break;

		default:
		{
			gLogger->logMsgF("Unhandled ObjectType in WorldManager::addObject: PRId32",MSG_HIGH,object->getType());
			// Please, when adding new stufff, at least take the time to add a stub for that type.
			// Better fail always, than have random crashes.
			assert(false && "WorldManager::addObject Unhandled ObjectType");
		}
		break;
	}
	return true;
}


//======================================================================================================================
//
// removes an object from the world
//

void WorldManager::destroyObjectForKnownPlayers(Object* object)
{

	PlayerObjectSet* knownPlayers = object->getKnownPlayers();
	PlayerObjectSet::iterator it = knownPlayers->begin();
	while(it != knownPlayers->end())
	{
		PlayerObject* targetObject = (*it);
		gMessageLib->sendDestroyObject(object->getId(),targetObject);
		targetObject->removeKnownObject(object);
		++it;
	}
	object->destroyKnownObjects();
	
}

//======================================================================================================================
//
// creates an object in the world around a player
//

void WorldManager::createObjectinWorld(PlayerObject* player, Object* object)
{
	//create it for players around us
	createObjectForKnownPlayers(player->getKnownPlayers(),object);
			
	//create it for us
	gMessageLib->sendCreateObject(object,player);
	player->addKnownObjectSafe(object);
	object->addKnownObjectSafe(player);
}

//======================================================================================================================
//
//make sure we scale down viewing range under high load
float WorldManager::_GetMessageHeapLoadViewingRange()
{
	uint32 heapWarningLevel = gMessageFactory->HeapWarningLevel();
	float view = (float)gWorldConfig->getPlayerViewingRange();

	if(gMessageFactory->getHeapsize() > 90.0)
		return 16.0;

	//just send everything we have
	if(heapWarningLevel < 3)
		return view; 
	else
	if(heapWarningLevel < 4)
		return 96.0;
	else
	if (heapWarningLevel < 5)
	{
		return 64.0;
	}
	else
	if (heapWarningLevel < 8)
	{
		return 32.0;
	}
	else
	if (heapWarningLevel <= 10)
	{
		return 16.0;
	}
	else
	if (heapWarningLevel > 10)
		return 8.0;

	return (float)gWorldConfig->getPlayerViewingRange();
}

//======================================================================================================================
//
// creates an object in the world queries the si to find relevant players
// generally querying the si is slow so beware to use this

void WorldManager::createObjectinWorld(Object* object)
{
	float				viewingRange	= _GetMessageHeapLoadViewingRange();
	ObjectSet inRangeObjects;
	mSpatialIndex->getObjectsInRange(object,&inRangeObjects,(ObjType_Player),viewingRange);

	// query the according qtree, if we are in one
	if(object->getSubZoneId())
	{
		if(QTRegion* region = getQTRegion(object->getSubZoneId()))
		{
			
			Anh_Math::Rectangle qRect;

			if(!object->getParentId())
			{
				qRect = Anh_Math::Rectangle(object->mPosition.x - viewingRange,object->mPosition.z - viewingRange,viewingRange * 2,viewingRange * 2);
			}
			else
			{
				CellObject*		cell		= dynamic_cast<CellObject*>(getObjectById(object->getParentId()));
				
				
				if(BuildingObject* house	= dynamic_cast<BuildingObject*>(getObjectById(cell->getParentId())))
				{
					qRect = Anh_Math::Rectangle(house->mPosition.x - viewingRange,house->mPosition.z - viewingRange,viewingRange * 2,viewingRange * 2);
				}
			}

			region->mTree->getObjectsInRange(object,&inRangeObjects,ObjType_Player,&qRect);
		}
	}

	// iterate through the results
	ObjectSet::iterator it = inRangeObjects.begin();

	while(it != inRangeObjects.end())
	{
		PlayerObject* player = dynamic_cast<PlayerObject*> (*it);
		if(player)
		{
			// send create for the type of object
			if (object->getPrivateOwner())	//what is this about ?? does it concern instances ????
			{
				if (object->isOwnedBy(player))
				{
					gMessageLib->sendCreateObject(object,player);
					object->addKnownObjectSafe(player);
					player->addKnownObjectSafe(object);
				}
			}
			else
			{
				gMessageLib->sendCreateObject(object,player);
				
				object->addKnownObjectSafe(player);
				player->addKnownObjectSafe(object);
			}
		}
		++it;
	}
	
}

//======================================================================================================================
//
// creates an object from the world
//

void WorldManager::createObjectForKnownPlayers(PlayerObjectSet* knownPlayers, Object* object)
{

	PlayerObjectSet::iterator it = knownPlayers->begin();
	while(it != knownPlayers->end())
	{	
		PlayerObject* targetObject = (*it);
		gMessageLib->sendCreateObject(object,targetObject);
		targetObject->addKnownObjectSafe(object);
		object->addKnownObjectSafe(targetObject);
		++it;
	}
	
}

//======================================================================================================================
//
// removes an object from the world
//

void WorldManager::destroyObject(Object* object)
{

	switch(object->getType())
	{
		case ObjType_Player:
		{
			//destroys knownObjects in the destructor
			PlayerObject* player = dynamic_cast<PlayerObject*>(object);

			// moved most of the code to the players destructor



			// onPlayerLeft event, notify scripts
			string params;
			params.setLength(sprintf(params.getAnsi(),"%s %s %u",getPlanetNameThis(),player->getFirstName().getAnsi(),static_cast<uint32>(mPlayerAccMap.size())));

			mWorldScriptsListener.handleScriptEvent("onPlayerLeft",params);
			// gLogger->logMsg("WorldManager::destroyObject: Player Client set to NULL");
			delete player->getClient();
			player->setClient(NULL);
			player->setConnectionState(PlayerConnState_Destroying);
		}
		break;
		case ObjType_NPC:
		case ObjType_Creature:
		{
			CreatureObject* creature = dynamic_cast<CreatureObject*>(object);

			// remove any timers we got running
			removeCreatureHamToProcess(creature->getHam()->getTaskId());

			// remove from cell / SI
			if (!object->getParentId())
			{
				// Not all objects-creatures of this type are points.
				if(creature->getSubZoneId())
				{
					if(QTRegion* region = getQTRegion(creature->getSubZoneId()))
					{
						creature->setSubZoneId(0);
						region->mTree->removeObject(creature);
					}
				}
				else
				{
					mSpatialIndex->RemovePoint(object->getId(),object->mPosition.x,object->mPosition.z);
				}
			}
			else
			{
				if(CellObject* cell = dynamic_cast<CellObject*>(getObjectById(object->getParentId())))
				{
					cell->removeObject(object);
				}
				else
				{
					//gLogger->logMsgF("WorldManager::destroyObject: couldn't find cell %"PRIu64"",MSG_HIGH,object->getParentId());
				}
			}

			// destroy known objects
			object->destroyKnownObjects();

			// if its a shuttle, remove it from the shuttle list
			if(creature->getCreoGroup() == CreoGroup_Shuttle)
			{
				ShuttleList::iterator shuttleIt = mShuttleList.begin();
				while(shuttleIt != mShuttleList.end())
				{
					if((*shuttleIt)->getId() == creature->getId())
					{
						mShuttleList.erase(shuttleIt);
						break;
					}

					++shuttleIt;
				}
			}
		}
		break;


		case ObjType_Structure:
		{
			// cave what do we do with player cities ??
			// then the parent Id should be the region object. shouldnt it????

			if(object->getSubZoneId())
			{
				if(QTRegion* region = getQTRegion(object->getSubZoneId()))
				{
					object->setSubZoneId(0);
					region->mTree->removeObject(object);
				}
			}
			else
			{
				mSpatialIndex->RemovePoint(object->getId(),object->mPosition.x,object->mPosition.z);
			}

			object->destroyKnownObjects();


			//remove it out of the worldmanagers structurelist now that it is deleted
			ObjectIDList::iterator itStruct = mStructureList.begin();
			while(itStruct != mStructureList.end())
			{
				if((*itStruct)==object->getId())
					itStruct = mStructureList.erase(itStruct);
				else
					itStruct++;
			}
			
		}
		break;

		case ObjType_Building:
		{

			BuildingObject* building = dynamic_cast<BuildingObject*>(object);
			if(building)
			{
				if(object->getSubZoneId())
				{
					if(QTRegion* region = getQTRegion(object->getSubZoneId()))
					{
						object->setSubZoneId(0);
						region->mTree->removeObject(object);
					}
				}
				else
				{	
					//mSpatialIndex->InsertRegion(key,building->mPosition.x,building->mPosition.z,building->getWidth(),building->getHeight());
					mSpatialIndex->RemoveRegion(object->getId(),object->mPosition.x-building->getWidth(),object->mPosition.z-building->getHeight(),object->mPosition.x+building->getWidth(),object->mPosition.z+building->getHeight());
				}

				//remove it out of the worldmanagers structurelist now that it is deleted
				ObjectIDList::iterator itStruct = mStructureList.begin();
				while(itStruct != mStructureList.end())
				{
					if((*itStruct)==object->getId())
						itStruct = mStructureList.erase(itStruct);
					else
						itStruct++;
				}
			
			}
			else
				gLogger->logMsgF("WorldManager::destroyObject: nearly did not remove: %"PRIu64"s knownObjectList",MSG_HIGH,object->getId());


			object->destroyKnownObjects();
		}
		break;

		case ObjType_Cell:
		{
			//a cell shouldnt have knownobjects ... -that should be checked to make sure it is true
			object->destroyKnownObjects();
		}
		break;

		case ObjType_Tangible:
		{
			if(TangibleObject* tangible = dynamic_cast<TangibleObject*>(object))
			{
				uint64 parentId = tangible->getParentId();

				if(parentId == 0)
				{
					mSpatialIndex->RemovePoint(tangible->getId(),tangible->mPosition.x,tangible->mPosition.z);
				}
				else
				{
					if(CellObject* cell = dynamic_cast<CellObject*>(getObjectById(parentId)))
					{
						cell->removeObject(object);
					}
					else
					{
						// Well, Tangible can have more kind of parents than just cells or SI. For example players or Inventory.
						// the tangible is owned by its containing object (please note exeption of inventory / player with equipped stuff)

						// however we should leave the object link in the worldmanagers Objectmap and only store references in the object
						// we will have great trouble finding items otherwise 

						//gLogger->logMsgF("WorldManager::destroyObject couldn't find cell %"PRIu64"",MSG_NORMAL,parentId);
					}
				}

			}
			else
			{
				gLogger->logMsgF("WorldManager::destroyObject: error removing : %"PRIu64"",MSG_HIGH,object->getId());
			}
			// destroy known objects
			object->destroyKnownObjects();
		}
		break;

		case ObjType_Region:
		{
			RegionMap::iterator it = mRegionMap.find(object->getId());

			if(it != mRegionMap.end())
			{
				mRegionMap.erase(it);
			}
			else
			{
				gLogger->logMsgF("Worldmanager::destroyObject: Could not find region %"PRIu64"",MSG_NORMAL,object->getId());
			}

			//camp regions are in here, too
			QTRegionMap::iterator itQ = mQTRegionMap.find(static_cast<uint32>(object->getId()));
			if(itQ != mQTRegionMap.end())
			{
				mQTRegionMap.erase(itQ);
				gLogger->logMsgF("Worldmanager::destroyObject: qt region %"PRIu64"",MSG_HIGH,object->getId());
			}

			object->destroyKnownObjects();

		}
		break;

		case ObjType_Intangible:
		{
			gLogger->logMsgF("Object of type ObjType_Intangible almost UNHANDLED in WorldManager::destroyObject:",MSG_HIGH);

			// intangibles are controllers / pets in the datapad
			// they are NOT in the world

			//we really shouldnt have any of thoose
			object->destroyKnownObjects();

		}
		break;

		default:
		{
			gLogger->logMsgF("Unhandled ObjectType in WorldManager::destroyObject: %u",MSG_HIGH,(uint32)(object->getType()));

			// Please, when adding new stufff, at least take the time to add a stub for that type.
			// Better fail always, than have random crashes.
			assert(false && "WorldManager::destroyObject Unhandled ObjectType");
		}
		break;
	}

	object->destroyKnownObjects();


	// finally delete it
	ObjectMap::iterator objMapIt = mObjectMap.find(object->getId());

	if(objMapIt != mObjectMap.end())
	{
		mObjectMap.erase(objMapIt);
	}
	else
	{
		gLogger->logMsgF("WorldManager::destroyObject: error removing from objectmap: %"PRIu64"",MSG_HIGH,object->getId());
	}
}


//======================================================================================================================
//
// simply erase an object ID out of the worlds ObjectMap *without* accessing the object
// I am aware this is somewhat a hack, though it is necessary, that the worldobjectlist can provide
// the objectcontroller with the IDs for the items in our datapad and inventory
// proper ObjectOwnership would normally require that these objects dont get added to the worlds object list
// perhaps it is possible to update the ObjectController at some later point to search the characters inventory / datapad
// but please be advised that the same problems do apply to items in houses / hoppers/ chests
// the objectcontroller can only provide them with a menu when it knows how to find the relevant Object
//

void WorldManager::eraseObject(uint64 key)
{											 

	// finally delete it
	ObjectMap::iterator objMapIt = mObjectMap.find(key);

	if(objMapIt != mObjectMap.end())
	{
		mObjectMap.erase(objMapIt);
	}
	else
	{
		gLogger->logMsgF("WorldManager::destroyObject: error removing from objectmap: %"PRIu64"",MSG_HIGH,key);
	}
}


//======================================================================================================================

void WorldManager::initObjectsInRange(PlayerObject* playerObject)
{
	float				viewingRange	= _GetMessageHeapLoadViewingRange();
	//if we are in a playerbuilding create the playerbuilding first
	//otherwise our items will not show when they are created before the cell
	if(CellObject* cell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(playerObject->getParentId())))
	{
		if(HouseObject* playerHouse = dynamic_cast<HouseObject*>(gWorldManager->getObjectById(cell->getParentId())))
		{
			//gLogger->logMsgF("create playerbuilding",MSG_HIGH);
			gMessageLib->sendCreateObject(playerHouse,playerObject);
			playerHouse->addKnownObjectSafe(playerObject);
			playerObject->addKnownObjectSafe(playerHouse);	
		}
	}



	// we still query for players here, cause they are found through the buildings and arent kept in a qtree
	ObjectSet inRangeObjects;
	mSpatialIndex->getObjectsInRange(playerObject,&inRangeObjects,(ObjType_Player | ObjType_Tangible | ObjType_NPC | ObjType_Creature | ObjType_Building | ObjType_Structure ),viewingRange);

	// query the according qtree, if we are in one
	if(playerObject->getSubZoneId())
	{
		if(QTRegion* region = getQTRegion(playerObject->getSubZoneId()))
		{
			Anh_Math::Rectangle qRect;

			if(!playerObject->getParentId())
			{
				qRect = Anh_Math::Rectangle(playerObject->mPosition.x - viewingRange,playerObject->mPosition.z - viewingRange,viewingRange * 2,viewingRange * 2);
			}
			else
			{
				CellObject*		cell		= dynamic_cast<CellObject*>(getObjectById(playerObject->getParentId()));
				
				
				if(BuildingObject* house	= dynamic_cast<BuildingObject*>(getObjectById(cell->getParentId())))
				{
					qRect = Anh_Math::Rectangle(house->mPosition.x - viewingRange,house->mPosition.z - viewingRange,viewingRange * 2,viewingRange * 2);
				}
			}

			region->mTree->getObjectsInRange(playerObject,&inRangeObjects,ObjType_Player,&qRect);
		}
	}

	// iterate through the results
	ObjectSet::iterator it = inRangeObjects.begin();

	while(it != inRangeObjects.end())
	{
		Object* object = (*it);

		// send create for the type of object
		if (object->getPrivateOwner())	//what is this about ?? does it concern instances ????
		{
			if (object->isOwnedBy(playerObject))
			{
				gMessageLib->sendCreateObject(object,playerObject);
				object->addKnownObjectSafe(playerObject);
				playerObject->addKnownObjectSafe(object);
			}
		}
		else
		{
			gMessageLib->sendCreateObject(object,playerObject);
				
			object->addKnownObjectSafe(playerObject);
			playerObject->addKnownObjectSafe(object);
			
		}
		++it;
	}
}

//======================================================================================================================

void WorldManager::_loadAllObjects(uint64 parentId)
{
	int8	sql[2048];
	WMAsyncContainer* asynContainer = new(mWM_DB_AsyncPool.ordered_malloc()) WMAsyncContainer(WMQuery_AllObjectsChildObjects);

	sprintf(sql,"(SELECT \'terminals\',terminals.id FROM terminals INNER JOIN terminal_types ON (terminals.terminal_type = terminal_types.id)"
				" WHERE (terminal_types.name NOT LIKE 'unknown') AND (terminals.parent_id = %"PRIu64") AND (terminals.planet_id = %"PRIu32"))"
				" UNION (SELECT \'containers\',containers.id FROM containers INNER JOIN container_types ON (containers.container_type = container_types.id)"
				" WHERE (container_types.name NOT LIKE 'unknown') AND (containers.parent_id = %"PRIu64") AND (containers.planet_id = %u))"
				" UNION (SELECT \'ticket_collectors\',ticket_collectors.id FROM ticket_collectors WHERE (parent_id=%"PRIu64") AND (planet_id=%u))"
				" UNION (SELECT \'persistent_npcs\',persistent_npcs.id FROM persistent_npcs WHERE (parentId=%"PRIu64") AND (planet_id = %"PRIu32"))"
				" UNION (SELECT \'shuttles\',shuttles.id FROM shuttles WHERE (parentId=%"PRIu64") AND (planet_id = %"PRIu32"))"
				" UNION (SELECT \'items\',items.id FROM items WHERE (parent_id=%"PRIu64") AND (planet_id = %"PRIu32"))"
				" UNION (SELECT \'resource_containers\',resource_containers.id FROM resource_containers WHERE (parent_id=%"PRIu64") AND (planet_id = %"PRIu32"))",
				parentId,mZoneId,parentId,mZoneId,parentId,mZoneId,parentId,mZoneId,parentId
				,mZoneId,parentId,mZoneId,parentId,mZoneId);

	mDatabase->ExecuteSqlAsync(this,asynContainer,sql);

	//gConfig->read<float>("FillFactor"
}

bool WorldManager::_handleGeneralObjectTimers(uint64 callTime, void* ref)
{
	CreatureObjectDeletionMap::iterator it = mCreatureObjectDeletionMap.begin();
	while (it != mCreatureObjectDeletionMap.end())
	{
		//  The timer has expired?
		if (callTime >= ((*it).second))
		{
			// Is it a valid object?
			CreatureObject* creature = dynamic_cast<CreatureObject*>(getObjectById((*it).first));
			if (creature)
			{
				// Yes, handle it. We may put up a copy of this npc...
				NpcManager::Instance()->handleExpiredCreature((*it).first);
				this->destroyObject(creature);
				mCreatureObjectDeletionMap.erase(it++);
			}
			else
			{
				// Remove the invalid object...from this list.
				mCreatureObjectDeletionMap.erase(it++);
			}
		}
		else
		{
			++it;
		}
	}

	PlayerObjectReviveMap::iterator reviveIt = mPlayerObjectReviveMap.begin();
	while (reviveIt != mPlayerObjectReviveMap.end())
	{
		//  The timer has expired?
		if (callTime >= ((*reviveIt).second))
		{

			PlayerObject* player = dynamic_cast<PlayerObject*>(getObjectById((*reviveIt).first));
			if (player)
			{
				// Yes, handle it.
				// Send the player to closest cloning facility.
				player->cloneAtNearestCloningFacility();

				// The cloning request removes itself from here, have to restart the iteration.
				reviveIt = mPlayerObjectReviveMap.begin();
			}
			else
			{
				// Remove the invalid object...
				mPlayerObjectReviveMap.erase(reviveIt++);
			}
		}
		else
		{
			++reviveIt;
		}
	}
	return (true);
}

//======================================================================================================================
//
//	getNearest Terminal
//

Object* WorldManager::getNearestTerminal(PlayerObject* player, TangibleType terminalType, float searchrange)
{


	//this will get the nearest terminal in the world - we need to check playerbuildings, too
	ObjectSet		inRangeObjects;
	this->getSI()->getObjectsInRange(player,&inRangeObjects,(ObjType_Tangible|ObjType_Building),searchrange);//range is debateable

	ObjectSet::iterator it = inRangeObjects.begin();
	
	float	range = 0.0;
	Object* nearestTerminal = NULL;

	while(it != inRangeObjects.end())
	{

		Terminal* terminal = dynamic_cast<Terminal*> (*it);
		if(terminal&&(terminal->getTangibleType() == terminalType))
		{
            float nr = glm::distance(terminal->mPosition, player->mPosition);
			//double check the distance
			if((nearestTerminal && (nr < range ))||(!nearestTerminal))
			{
				range = nr;
				nearestTerminal = terminal;
			}
		}
		else
		if(BuildingObject* building = dynamic_cast<BuildingObject*> (*it))
		{
			//iterate through the structure and look for terminals
			ObjectList list = building->getAllCellChilds();
			ObjectList::iterator cellChildsIt = list.begin();

			while(cellChildsIt != list.end())
			{
				Object* cellChild = (*cellChildsIt);

				uint32 tmpType = cellChild->getType();

				if((tmpType & ObjType_Tangible) == static_cast<uint32>(ObjType_Tangible))
				{
					
					Terminal* terminal = dynamic_cast<Terminal*> (cellChild);
					if(terminal&&(terminal->getTangibleType() == terminalType))
					{
                        float nr = glm::distance(terminal->mPosition, player->mPosition);
						//double check the distance
						if((nearestTerminal && (nr < range))||(!nearestTerminal))
						{
							range = nr;
							nearestTerminal = terminal;
						}
					}
				}

				++cellChildsIt;
			}
		}

		++it;
	}
	return nearestTerminal;
}