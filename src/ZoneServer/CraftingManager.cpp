#include "CraftingManager.h"
#include "CraftingSessionFactory.h"
#include "CraftingTool.h"
#include "DatabaseManager/Database.h"

//Needs Refactored
#include "PlayerObject.h"
#include "Inventory.h"
#include "WorldManager.h" //ICK!!!
#include "MessageLib/MessageLib.h" //ICKICKICK!!!!


CraftingManager* CraftingManager::mSingleton = NULL;

CraftingManager* CraftingManager::Init(Database* database)
{
	if(!mSingleton)
		mSingleton = new CraftingManager(database);

	return mSingleton;
}

CraftingManager* CraftingManager::getSingleton()
{
	return mSingleton;
}

void CraftingManager::DestroyInstance()
{
	delete mSingleton;
}

CraftingManager::CraftingManager(Database* database)
{
	mCraftingSessionFactory = new CraftingSessionFactory(database);
	mDatabase = database;
}

bool CraftingManager::Process(uint64 callTime,void* ref)
{
	_handleCraftToolTimers(callTime);
	
	return(true);
}



//======================================================================================================================
//
// update busy crafting tools, called every 2 seconds
//
void CraftingManager::_handleCraftToolTimers(uint64 callTime)
{
	CraftTools::iterator it = mBusyCraftTools.begin();

	while(it != mBusyCraftTools.end())
	{
		CraftingTool*	tool	=	dynamic_cast<CraftingTool*>((*it));
		if(!tool)
		{
			gLogger->logMsgF("WorldManager::_handleCraftToolTimers missing crafting tool :(",MSG_NORMAL);
			it = mBusyCraftTools.erase(it);
			continue;
		}

		PlayerObject*	player	=	dynamic_cast<PlayerObject*>(gWorldManager->getObjectById(tool->getParentId() - 1));
		Item*			item	= tool->getCurrentItem();

		if(player)
		{
			// we done, create the item
			if(!tool->updateTimer(callTime))
			{
				// add it to the world, if it holds an item
				if(item)
				{
					Inventory* temp =  dynamic_cast<Inventory*>(player->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory));
					if(!temp) continue;

					item->setParentId(temp->getId());
					dynamic_cast<Inventory*>(player->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory))->addObject(item);
					gWorldManager->addObject(item,true);

					gMessageLib->sendCreateTangible(item,player);

					tool->setCurrentItem(NULL);
				}
				//in case of logout/in interplanetary travel it will be in the inventory already

				gMessageLib->sendUpdateTimer(tool,player);

				it = mBusyCraftTools.erase(it);
				tool->setAttribute("craft_tool_status","@crafting:tool_status_ready");
				mDatabase->ExecuteSqlAsync(0,0,"UPDATE item_attributes SET value='@crafting:tool_status_ready' WHERE item_id=%"PRIu64" AND attribute_id=18",tool->getId());

				tool->setAttribute("craft_tool_time",boost::lexical_cast<std::string>(tool->getTimer()));
				gWorldManager->getDatabase()->ExecuteSqlAsync(0,0,"UPDATE item_attributes SET value='%i' WHERE item_id=%"PRIu64" AND attribute_id=%u",tool->getId(),tool->getTimer(),AttrType_CraftToolTime);

				continue;
			}
			// update the time display
			gMessageLib->sendUpdateTimer(tool,player);

			tool->setAttribute("craft_tool_time",boost::lexical_cast<std::string>(tool->getTimer()));
			//gLogger->logMsgF("timer : %i",MSG_HIGH,tool->getTimer());
			gWorldManager->getDatabase()->ExecuteSqlAsync(0,0,"UPDATE item_attributes SET value='%i' WHERE item_id=%"PRIu64" AND attribute_id=%u",tool->getId(),tool->getTimer(),AttrType_CraftToolTime);
		}

		++it;
	}
}

//======================================================================================================================

void CraftingManager::addBusyCraftTool(CraftingTool* tool)
{
	mBusyCraftTools.push_back(tool);
}

//======================================================================================================================

void CraftingManager::removeBusyCraftTool(CraftingTool* tool)
{
	CraftTools::iterator it = mBusyCraftTools.begin();
	CraftTools::iterator tEnd = mBusyCraftTools.end();

	while(it != tEnd)
	{
		if((*it) == tool)
		{
			mBusyCraftTools.erase(it);
			break;
		}

		++it;
	}
}

CraftingSession*	CraftingManager::createSession(Anh_Utils::Clock* clock,PlayerObject* player,CraftingTool* tool,CraftingStation* station,uint32 expFlag)
{
	return mCraftingSessionFactory->createSession(clock, player, tool, station, expFlag);
}

void CraftingManager::destroySession(CraftingSession* session)
{
	mCraftingSessionFactory->destroySession(session);
}