/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/
#include "ActiveConversation.h"
#include "BuildingObject.h"
#include "HouseObject.h"

#include "CellObject.h"
#include "ConversationManager.h"
#include "Heightmap.h"
#include "NPCObject.h"
#include "ObjectController.h"
#include "ObjectControllerOpcodes.h"
#include "ObjectControllerCommandMap.h"
#include "PlayerObject.h"
#include "FactoryObject.h"
#include "QuadTree.h"
#include "Tutorial.h"
#include "WorldConfig.h"
#include "WorldManager.h"
#include "Vehicle.h"
#include "ZoneTree.h"

#include "MessageLib/MessageLib.h"
#include "LogManager/LogManager.h"
#include "DatabaseManager/Database.h"
#include "DatabaseManager/DatabaseResult.h"
#include "DatabaseManager/DataBinding.h"
#include "Common/Message.h"
#include "Common/MessageFactory.h"
#include "Utils/clock.h"

#include <cassert>

//=============================================================================
//
// position update in world
//

void ObjectController::handleDataTransform(Message* message,bool inRangeUpdate)
{
	PlayerObject*			player = dynamic_cast<PlayerObject*>(mObject);

	if (!player)
	{
		gLogger->logMsgF("ObjectController::handleDataTransform Object is NOT A PLAYER, id = %"PRIu64"", MSG_HIGH, mObject->getId());
		return;
	}

    glm::vec3		pos;
    glm::quat       dir;
	uint32			inMoveCount;
	uint32			tickCount;
	float			speed;
	bool updateAll = false;

	// get tick and move counters
	tickCount	= message->getUint32();
	inMoveCount = message->getUint32();

	// gLogger->logMsg("ObjectController::handleDataTransform");
	//uint64 localTimeStart = Anh_Utils::Clock::getSingleton()->getLocalTime();

	// only process if its in sequence
	if(player->getInMoveCount() >= inMoveCount)
	{
		return;
	}
	//uint32 ticks = tickCount - player->getClientTickCount();

	// update tick and move counters...
	player->setLastMoveTick(tickCount);
	player->setClientTickCount(tickCount);

	player->setInMoveCount(inMoveCount);

	if(player->checkIfMounted() && player->getMount())
	{
		//Player is mounted lets update his mount too
		player->getMount()->setLastMoveTick(tickCount);
		//player->getMount()->setInMoveCount((inMoveCount+1));
		player->getMount()->setInMoveCount((inMoveCount)); // + 1 or nor does not matter, as long as we update inMoveCount.
	}


	// get new direction, position and speed
	dir.x = message->getFloat();
	dir.y = message->getFloat();	 
	dir.z = message->getFloat();
	dir.w = message->getFloat();

	pos.x = message->getFloat();
	pos.y = message->getFloat();
	pos.z = message->getFloat();
	speed  = message->getFloat();

	// gLogger->logMsgF("Position outside = %.2f, %.2f, %.2f",MSG_NORMAL, pos.x,  pos.y, pos.z);
	/*
	if (Heightmap::isHeightmapCacheAvaliable())
	{
	gLogger->logMsgF("Heightmap value = %.2f",MSG_NORMAL, Heightmap::Instance()->getCachedHeightAt2DPosition(pos.x, pos.z));
	}
	*/

	// gLogger->logMsgF("Direction = %f, %f, %f, %f",MSG_NORMAL, dir.x, dir.y, dir.z, dir.w);

	// stop entertaining, if we were
	// important is, that if we move we change our posture to NOT skill animating anymore!
	// so only stop entertaining when we are performing and NOT skillanimationg
	if(player->getPerformingState() != PlayerPerformance_None && player->getPosture() != CreaturePosture_SkillAnimating)
	{
		gEntertainerManager->stopEntertaining(player);
	}

	// if we just left a building
	if(player->getParentId() != 0)
	{
		updateAll = true;

		// Testing with 4 for add and 0 for remove.
		// Remove us from previous cell.
		gMessageLib->broadcastContainmentMessage(player->getId(),player->getParentId(),0,player);

		// remove us from the last cell we were in
		if(CellObject* cell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(player->getParentId())))
		{
			cell->removeObject(player);
		}
		else
		{
			gLogger->logMsgF("Error removing %"PRIu64" from cell(%"PRIu64")",MSG_HIGH,player->getId(),player->getParentId());
		}

		// we are outside again
		player->setParentId(0);
		player->mPosition = pos;
		// Add us to the world.
		gMessageLib->broadcastContainmentMessage(player->getId(),0,4,player);

		// add us to the qtree

		if(QTRegion* newRegion = mSI->getQTRegion((double)pos.x,(double)pos.z))
		{
			player->setSubZoneId((uint32)newRegion->getId());
			newRegion->mTree->addObject(player);
		}
		else
		{
			// we should never get here !
			gLogger->logMsg("ObjController::handleDataTransform: could not find zone region in map");
			gLogger->logMsg("ObjController:: probably a bot : %i64u",static_cast<int>(player->getId()));

			// hammertime !
			//muglies botter sometimes sends us weird positions
			//however other 3rd party tools might do the same

			gWorldManager->addDisconnectedPlayer(player);
			return;
		}
		// Inform tutorial about cell change.
		if (gWorldConfig->isTutorial())
		{
			player->getTutorial()->setCellId(0);
		}

	}
	else //we are not in a building
	{
		// we should be in a qt at this point
		// get the qt of the new position
		if(QTRegion* newRegion = mSI->getQTRegion((double)pos.x,(double)pos.z))
		{
			// we didnt change so update the old one
			if((uint32)newRegion->getId() == player->getSubZoneId())
			{
				// this also updates the players position
				newRegion->mTree->updateObject(player,pos);
				//If our player is mounted lets update his mount aswell
				if(player->checkIfMounted() && player->getMount())
				{
					newRegion->mTree->updateObject(player->getMount(),pos);
				}
			}
			else
			{
				updateAll = true;

				gLogger->logMsg("ObjController::DataTransform: Changing subzone");
				// remove from old
				if(QTRegion* oldRegion = gWorldManager->getQTRegion(player->getSubZoneId()))
				{
					oldRegion->mTree->removeObject(player);
					//If our player is mounted lets update his mount aswell
					if(player->checkIfMounted() && player->getMount())
					{
						oldRegion->mTree->removeObject(player->getMount());
					}
				}

				// update players position
				player->mPosition = pos;
				//If our player is mounted lets update his mount aswell
				if(player->checkIfMounted() && player->getMount())
				{
					player->getMount()->mPosition = pos;
				}

				// put into new
				player->setSubZoneId((uint32)newRegion->getId());
				newRegion->mTree->addObject(player);
				//If our player is mounted lets update his mount aswell
				if(player->checkIfMounted() && player->getMount())
				{
					player->getMount()->setSubZoneId((uint32)newRegion->getId());
					newRegion->mTree->addObject(player->getMount());
				}

			}
		}
		else
		{
			// we should never get here !
			gLogger->logMsg("ObjController::DataTransform: could not find zone region in map");

			gLogger->logMsg("ObjController:: probably a bot : %I64u",static_cast<int>(player->getId()));

			// hammertime !
			// muglies botter sometimes sends us weird positions  with X or Y far out of possible regions
			// however other 3rd party tools might do the same
			// we need to get rid of the client at this point nad probably should ban the player / add him to
			// a monitoring list when the coordinates were indeed out of bounds

			gWorldManager->addDisconnectedPlayer(player);
			return;
		}
	}

	player->mDirection = dir;
	player->setCurrentSpeed(speed);

	//If our player is mounted lets update his mount aswell
	if(player->checkIfMounted() && player->getMount())
	{
		player->getMount()->mDirection = dir;
		player->getMount()->setCurrentSpeed(speed);
	}


	// destroy the instanced instrument if out of range
	if (player->getPlacedInstrumentId())
	{
		if (!gWorldManager->objectsInRange(player->getId(), player->getPlacedInstrumentId(), 5.0))
		{
			if (Item* item = dynamic_cast<Item*>(gWorldManager->getObjectById(player->getPlacedInstrumentId())))
			{
				destroyObject(item->getId());
			}
		}
	}

	// Terminate active conversation with npc if to far away (trainers only so far).
	ActiveConversation* ac = gConversationManager->getActiveConversation(player->getId());
	if (ac != NULL)
	{
		// We do have a npc conversation going.
		if (!gWorldManager->objectsInRange(player->getId(), (ac->getNpc())->getId(), 11.0))
		{
			// Terminate conversation, since we are out of range.
			gMessageLib->sendSystemMessage(player,L"","system_msg","out_of_range");
			gConversationManager->stopConversation(player, true);			// We will get the current dialog text in a chat bubble, only seen by me. Impressive :)
		}
	}

	if (updateAll)
	{
		// Update our world.
		playerWorldUpdate(true);

		// Speed up the timed update, if any pending.
		gWorldManager->addPlayerMovementUpdateTime(player, 250);
	}
	else
	{
		if (!gWorldConfig->isInstance())
		{

			//If player is mounted... move his mount too!
			if(player->checkIfMounted() && player->getMount())
			{
				//gMessageLib->sendDataTransform(player->getMount());
				gMessageLib->sendUpdateTransformMessage(player->getMount());
			}
			else
			{
				// send out position updates to known players
				// please note that these updates mess up our dance performance
				if(player->getPerformingState() == PlayerPerformance_None)
				{
					gMessageLib->sendUpdateTransformMessage(player);
				}
		

			}

		}
		else
		{
			// send out position updates to known players in group or self only
			gMessageLib->sendUpdateTransformMessage(player, player);
		}
	}

	 //uint64 localTimeEnd = Anh_Utils::Clock::getSingleton()->getLocalTime();
	 //gLogger->logMsgF("Exec time PRId32",MSG_NORMAL, localTimeEnd - localTimeStart);
}

//=============================================================================
//
// position update in cell
//

void ObjectController::handleDataTransformWithParent(Message* message,bool inRangeUpdate)
{
	// FIXME: for now assume we only get messages from players
	PlayerObject*	player = dynamic_cast<PlayerObject*>(mObject);
    glm::vec3       pos;
    glm::quat       dir;
	uint32          inMoveCount;
	uint32			tickCount;
	uint64			parentId;
	float			speed;
	bool			updateAll = false;

	// get tick and move counters
	tickCount	= message->getUint32();
	inMoveCount = message->getUint32();

	// only process if its in sequence
	if (player->getInMoveCount() <= inMoveCount)
	{
		uint64 oldParentId = player->getParentId();

		//uint32 ticks = tickCount - player->getClientTickCount();

		// update tick and move counters
		player->setClientTickCount(tickCount);
		player->setInMoveCount(inMoveCount);

		// get new direction, position, parent and speed
		parentId = message->getUint64();
		dir.x = message->getFloat();
		dir.y = message->getFloat();
		dir.z = message->getFloat();
		dir.w = message->getFloat();
		pos.x = message->getFloat();
		pos.y = message->getFloat();
		pos.z = message->getFloat();
		speed  = message->getFloat();


		// gLogger->logMsgF("Position inside = %f, %f, %f",MSG_NORMAL, pos.x,  pos.y, pos.z);
		// gLogger->logMsgF("Direction = %f, %f, %f, %f",MSG_NORMAL, dir.x, dir.y, dir.z, dir.w);

		// stop entertaining, if we were
		if(player->getPerformingState() != PlayerPerformance_None && player->getPosture() != CreaturePosture_SkillAnimating)
		{
			gEntertainerManager->stopEntertaining(player);
		}

		// if we changed cell
		if (oldParentId != parentId)
		{

			

			CellObject* cell = NULL;

			// gLogger->logMsgF("We changed cell from (%"PRIu64") to (%"PRIu64")",MSG_NORMAL, oldParentId, parentId);

			// Remove us from whatever we where in before.
			// (4 for add and 0 for remove)
			gMessageLib->broadcastContainmentMessage(player->getId(),oldParentId,0,player);

			// only remove us from si, if we just entered the building
			if (oldParentId != 0)
			{
				if((cell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(oldParentId))))
				{
					cell->removeObject(player);
					// Done above.. gMessageLib->broadcastContainmentMessage(player->getId(),parentId,4,player);
				}
				else
				{
					gLogger->logMsgF("Error removing %"PRIu64" from cell(%"PRIu64")",MSG_NORMAL,player->getId(),oldParentId);
				}
			}
			else
			{
				updateAll = true;		// We just entered the building.

				// remove us from qt
				if(player->getSubZoneId())
				{
					if(QTRegion* region = gWorldManager->getQTRegion(player->getSubZoneId()))
					{
						player->setSubZoneId(0);
						region->mTree->removeObject(player);
						//If our player is mounted lets update his mount aswell
						if(player->checkIfMounted() && player->getMount())
						{
							player->getMount()->setSubZoneId(0);
							region->mTree->removeObject(player->getMount());

							//Can't ride into a building with a mount! :-p
							//However, its easy to do so we have handling incase the client is tricked.


							// the vehicle is the INTANGIBLE Datapad Controller
							// the *vehicle* itself is the BODY
							if(Vehicle* datapad_pet = dynamic_cast<Vehicle*>(gWorldManager->getObjectById(player->getMount()->getPetController())))
							{
								datapad_pet->dismountPlayer();
								datapad_pet->store();
							}
						}
					}
				}
			}

			// put us into new one
			gMessageLib->broadcastContainmentMessage(player->getId(),parentId,4,player);
			if((cell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(parentId))))
			{

				cell->addObjectSecure(player);

				// Inform tutorial about cell change.
				if (gWorldConfig->isTutorial())
				{
					player->getTutorial()->setCellId(parentId);
					// gLogger->logMsgF("handleDataTransformWithParent: Adding %"PRIu64" to cell(%"PRIu64")",MSG_NORMAL,player->getId(),parentId);
				}
			}
			else
			{
				gLogger->logMsgF("Error adding %"PRIu64" to cell(%"PRIu64")",MSG_NORMAL,player->getId(),parentId);
			}
		}

		// update the player
		player->setParentId(parentId);
		player->mDirection = dir;
		player->mPosition  = pos;
		player->setCurrentSpeed(speed);

		// destroy the instanced instrument if out of range
		if (player->getPlacedInstrumentId())
		{
			if (!gWorldManager->objectsInRange(player->getId(), player->getPlacedInstrumentId(), 5.0))
			{
				if (Item* item = dynamic_cast<Item*>(gWorldManager->getObjectById(player->getPlacedInstrumentId())))
				{
					destroyObject(item->getId());
				}
			}
		}

		// Terminate active conversation with npc if to far away (trainers only so far).
		ActiveConversation* ac = gConversationManager->getActiveConversation(player->getId());
		if (ac != NULL)
		{
			// We do have a npc conversation going.
			if (!gWorldManager->objectsInRange(player->getId(), (ac->getNpc())->getId(), 11.0))
			{
				// Terminate conversation, since we are out of range.
				gMessageLib->sendSystemMessage(player,L"","system_msg","out_of_range");
				gConversationManager->stopConversation(player, true);			// We will get the current dialog text in a chat bubble, only seen by me. Impressive :)
			}
		}

		if (updateAll)
		{
			// Update our world.
			playerWorldUpdate(true);

			// Speed up the timed update, if any pending.
			gWorldManager->addPlayerMovementUpdateTime(player, 250);
		}
		else
		{
			if (!gWorldConfig->isInstance())
			{
				// send out updates
				gMessageLib->sendUpdateTransformMessageWithParent(player);
			}
			else
			{
				// send out position updates to known players in group or self only
				gMessageLib->sendUpdateTransformMessageWithParent(player, player);
			}
		}
	}
}


//=========================================================================================
//

float ObjectController::_GetMessageHeapLoadViewingRange()
{
	uint32 heapWarningLevel = gMessageFactory->HeapWarningLevel();

	if(gMessageFactory->getHeapsize() > 99.0)
		return 16.0;

	//just send everything we have
	if(heapWarningLevel < 3)
		return (float)gWorldConfig->getPlayerViewingRange();
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

//=========================================================================================
//


void ObjectController::_findInRangeObjectsOutside(bool updateAll)
{
	PlayerObject*	player			= dynamic_cast<PlayerObject*>(mObject);

	//scale down viewing range when busy
	float			viewingRange	= _GetMessageHeapLoadViewingRange();

	// gLogger->logMsg("... _findInRangeObjectsOutside.");

	// query the rtree for non moving objects/objects in buildings
	// ObjectSet		inRangeObjects;

	// mSI->getObjectsInRange(player,&inRangeObjects,(ObjType_Player | ObjType_Tangible | ObjType_Creature | ObjType_NPC | ObjType_Building),viewingRange);
	// Using intersectsWithQuery(..)
	// NOTE: THIS USEAGE OF intersectsWithQuery(..) MUST BE CHECKED, SINCE IT SEEMS THAT WE GET TO MUCH / TO MANY OBJECTS !!!
	// mSI->getObjectsInRangeEx(player,&inRangeObjects,(ObjType_Player | ObjType_Tangible | ObjType_Creature | ObjType_NPC | ObjType_Building),viewingRange);

	// Make Set ready,
	mInRangeObjects.clear();
	mObjectSetIt = mInRangeObjects.begin();	// Will point to end of Set

	if(player->getSubZoneId())
	{
		if(QTRegion* region = gWorldManager->getQTRegion(player->getSubZoneId()))
		{
			// gLogger->logMsg("... in a region.");
			Anh_Math::Rectangle qRect = Anh_Math::Rectangle(player->mPosition.x - viewingRange,player->mPosition.z - viewingRange,viewingRange * 2,viewingRange * 2);

			// We need to find moving creatures also...
			region->mTree->getObjectsInRange(player,&mInRangeObjects,ObjType_Player | ObjType_NPC | ObjType_Creature | ObjType_Lair , &qRect);
		}
	}

	if (updateAll)
	{
		// gLogger->logMsg("UpdateAll.");

		// Doing this because we need the players from inside buildings too.
		mSI->getObjectsInRangeEx(player,&mInRangeObjects,(ObjType_Player | ObjType_NPC | ObjType_Creature), viewingRange);

		// This may be good when we standstill.
		mSI->getObjectsInRange(player,&mInRangeObjects,(ObjType_Tangible | ObjType_Building | ObjType_Lair | ObjType_Structure), viewingRange);

	}
	/*
	{
		// This may be good when we are moving around outside.
		mSI->getObjectsInRange(player,&mInRangeObjects,(ObjType_Player | ObjType_NPC | ObjType_Creature | ObjType_Tangible | ObjType_Building), viewingRange);
	}
	*/

	// Update the iterator to start of Set.
	mObjectSetIt = mInRangeObjects.begin();
}

//=========================================================================================
//

bool ObjectController::_updateInRangeObjectsOutside()
{
	PlayerObject*	player = dynamic_cast<PlayerObject*>(mObject);

	// We may wan't to limit the amount of messages sent in one session.
	uint32 updatedObjects = 0;
	const uint32 objectSendLimit = 50;

	while ((mObjectSetIt != mInRangeObjects.end()) && (updatedObjects < objectSendLimit))
	{
		//BEWARE Object is at this time possibly not valid anymore!
		//this actually causes a lot of crashes!!!!

		Object* object = dynamic_cast<Object*>(*mObjectSetIt);
		// Just simplified the code a little. Good find Schmunzel.

		// only add it if its also outside
		// see if its already observed, if yes, just send a position update out, if its a player
		if ((object) && (!player->checkKnownObjects(object)))
		{
			// send the according create for the type of object
#if defined(_MSC_VER)
			if (object->getId() > 0x0000000100000000)
#else
			if (object->getId() > 0x0000000100000000LLU)
#endif
			{
				if (object->getPrivateOwner())
				{
					if (object->isOwnedBy(player))
					{
						gMessageLib->sendCreateObject(object,player);
						player->addKnownObjectSafe(object);
						object->addKnownObjectSafe(player);

						//If player has a mount make sure add to its known objects
						//but is the mount even near us ???
						// does this even matter ?

						if(player->checkIfMountCalled() && player->getMount())
						{
							if(player->getMount()->getId() != object->getId())
							{
								player->getMount()->addKnownObjectSafe(object);
								object->addKnownObjectSafe(player->getMount());
							}
						}
						updatedObjects++;
					}
				}
				else
				{
					//if(!player->checkKnownObjects(object))
					//{
						gMessageLib->sendCreateObject(object,player);
						player->addKnownObjectSafe(object);
						object->addKnownObjectSafe(player);

						//If player has a mount make sure add to its known objects
						if(player->checkIfMountCalled() && player->getMount())
						{
							if(player->getMount()->getId() != object->getId())
							{
								player->getMount()->addKnownObjectSafe(object);
								object->addKnownObjectSafe(player->getMount());
							}
						}
					//}
					updatedObjects++;
				}
			}
		}
		++mObjectSetIt;
	}
	return (mObjectSetIt == mInRangeObjects.end());
}


//=========================================================================================
//
// Find the objects observed/known objects when inside
//
void ObjectController::_findInRangeObjectsInside(bool updateAll)
{
	PlayerObject*	player = dynamic_cast<PlayerObject*>(mObject);
	float			viewingRange = _GetMessageHeapLoadViewingRange();
	CellObject*		playerCell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(player->getParentId()));


	// Make Set ready,
	mInRangeObjects.clear();

	// Init the iterator.
	mObjectSetIt = mInRangeObjects.begin();

	// make sure we got a cell
	if (!playerCell)
	{
		gLogger->logMsg("ERROR: No playerCell.");
		return;
	}

	Object* object = gWorldManager->getObjectById(playerCell->getParentId());
	//BuildingObject* building = dynamic_cast<BuildingObject*>(object);
	BuildingObject* building = dynamic_cast<BuildingObject*>(object);
	// make sure we got a building
	if (!building)
	{
		
		gLogger->logMsg("ERROR: No building.");
		return;
	}


	// mInRangeObjectIndex = 0;

	if (updateAll)
	{
		// This is good to use when entering a building.
		mSI->getObjectsInRange(player,&mInRangeObjects,(ObjType_Player | ObjType_Tangible | ObjType_NPC | ObjType_Creature | ObjType_Building | ObjType_Structure),viewingRange);

		// query the qtree based on the buildings world position
		if (QTRegion* region = mSI->getQTRegion(building->mPosition.x,building->mPosition.z))
		{
			Anh_Math::Rectangle qRect = Anh_Math::Rectangle(building->mPosition.x - viewingRange,building->mPosition.z - viewingRange,viewingRange * 2,viewingRange * 2);

			// We need to find moving creatures outside...
			region->mTree->getObjectsInRange(player,&mInRangeObjects,ObjType_Player | ObjType_NPC | ObjType_Creature, &qRect);
		}
	}
	else
	{
		// This is good to use when running around inside a building.

		// We need to see player or creatures spawning inside.

		// Added ObjType_Tangible because Tutorial spawns ObjType_Tangible in a way we don't normally do.
		// If we need more speed in normal cases, just add a test for Tutorial and de-select ObjType_Tangible if not active.
		mSI->getObjectsInRange(player,&mInRangeObjects,(ObjType_Tangible | ObjType_Player | ObjType_Creature | ObjType_NPC),viewingRange);

		// query the qtree based on the buildings world position
		if (QTRegion* region = mSI->getQTRegion(building->mPosition.x,building->mPosition.z))
		{
			Anh_Math::Rectangle qRect = Anh_Math::Rectangle(building->mPosition.x - viewingRange,building->mPosition.z - viewingRange,viewingRange * 2,viewingRange * 2);

			// We need to find moving creatures outside...
			region->mTree->getObjectsInRange(player,&mInRangeObjects,ObjType_Player | ObjType_NPC | ObjType_Creature,&qRect);
		}
	}
	// Update the iterator to start of Set.
	mObjectSetIt = mInRangeObjects.begin();
}


//=========================================================================================
//
// Update the objects observed/known objects when inside
//

bool ObjectController::_updateInRangeObjectsInside()
{
	PlayerObject*	player = dynamic_cast<PlayerObject*>(mObject);
	CellObject*		playerCell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(player->getParentId()));

	// make sure we got a cell
	if (!playerCell)
	{
		gLogger->logMsgF("Error getting cell %"PRIu64" for %"PRIu64" type %u",MSG_NORMAL,player->getParentId(),player->getId(),player->getType());
		return true;	// We are done, nothing we can do...
	}

	// We may wan't to limit the amount of messages sent in one session.
	uint32 updatedObjects = 0;
	const uint32 objectSendLimit = 50;

	//what do we do if the object has been deleted in the meantime?
	//TODO
	while ((mObjectSetIt != mInRangeObjects.end()) && (updatedObjects < objectSendLimit))
	{
		// Object* object = (*mObjectSetIt);
		// Needed since object may be invalid due to the multi-session approach of this function.
		Object* object = dynamic_cast<Object*>(*mObjectSetIt);

		// Create objects that are in the same building as we are OR outside near the building.
		if ((object) && (!player->checkKnownObjects(object)))
		{
			bool validObject = true;	// Assume it's an object we shall add.

			// Object inside a building ?
			if (object->getParentId())
			{
				validObject = false;

				// Yes, get the objects cell.
				CellObject* objectCell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(object->getParentId()));
				if (objectCell)
				{
					// Are we in the same building?
					if (objectCell->getParentId() == playerCell->getParentId())
					{
						// We are in the same building as the object.
						validObject = true;
					}
				}
				else
				{
					gLogger->logMsgF("Error getting cell %"PRIu64" for %"PRIu64" type %u",MSG_NORMAL,object->getParentId(),object->getId(),object->getType());
				}
			}
			if (validObject)
			{
				// send the according create for the type of object
#if defined(_MSC_VER)
				if (object->getId() > 0x0000000100000000)
#else
				if (object->getId() > 0x0000000100000000LLU)
#endif
				{
					//if its an instance and per chance *our* instance
					if (object->getPrivateOwner()&&object->isOwnedBy(player))
					{
						gMessageLib->sendCreateObject(object,player);
						player->addKnownObjectSafe(object);
						object->addKnownObjectSafe(player);
						updatedObjects++;
		
					}
					else
					{
						//if(!player->checkKnownObjects(object))
						//{
							gMessageLib->sendCreateObject(object,player);
							player->addKnownObjectSafe(object);
							object->addKnownObjectSafe(player);
							updatedObjects++;
						//}
					}
				}
			}
		}
		++mObjectSetIt;
	}
	return (mObjectSetIt == mInRangeObjects.end());
}

//=========================================================================================
//
// destroy known objects not in range anymore
// compares the given list with the players known objects
//

bool ObjectController::_destroyOutOfRangeObjects(ObjectSet *inRangeObjects)
{
	//TODO: when a container gets out of range
	//we need to destroy the children, too!!!!!!!

	// iterate our knowns
	PlayerObject*				player			= dynamic_cast<PlayerObject*>(mObject);
	ObjectSet*					knownObjects	= player->getKnownObjects();
	ObjectSet::iterator			objIt			= knownObjects->begin();
	PlayerObjectSet*			knownPlayers	= player->getKnownPlayers();
	PlayerObjectSet::iterator	playerIt		= knownPlayers->begin();

	const uint32 objectDestroyLimit = 5000;

	// update players
	while(playerIt != knownPlayers->end())
	{
		PlayerObject* playerObject = (*playerIt);

		// if its not in the current inrange queries result, destroy it
		if(inRangeObjects->find(playerObject) == inRangeObjects->end())
		{
			// send a destroy to us
			gMessageLib->sendDestroyObject(playerObject->getId(),player);

			//If player is mounted destroy his mount too
			if(playerObject->checkIfMounted() && playerObject->getMount())
			{
				gMessageLib->sendDestroyObject(playerObject->getMount()->getId(),player);
				
				player->removeKnownObject(playerObject->getMount());
				playerObject->getMount()->removeKnownObject(player);
				
			}

			//send a destroy to him
			gMessageLib->sendDestroyObject(player->getId(),playerObject);

			//If we're mounted destroy our mount too
			if(player->checkIfMounted() && playerObject->getMount())
			{
				gMessageLib->sendDestroyObject(player->getMount()->getId(),playerObject);
				playerObject->removeKnownObject(player->getMount());
				player->getMount()->removeKnownObject(playerObject);
				
			}

			// we don't know each other anymore
			knownPlayers->erase(playerIt++);
			playerObject->removeKnownObject(player);


			continue;
		}
		++playerIt;
	}

	// We may want to limit the amount of messages sent in one session.
	uint32 messageCount = 0;

	// update objects
	while(objIt != knownObjects->end())
	{
		Object* object = (*objIt);

		// if its not in the current inrange queries result, destroy it
		if(inRangeObjects->find(object) == inRangeObjects->end())
		{

			if(object->getType() == ObjType_Structure)
			{
				if(FactoryObject* factory = dynamic_cast<FactoryObject*>(object))
				{
					//delete the hoppers contents
					TangibleObject* hopper = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(factory->getIngredientHopper()));
					if(hopper)
					{
							ObjectIDList*			ol = hopper->getObjects();
							ObjectIDList::iterator	it = ol->begin();

							while(it != ol->end())
							{
								TangibleObject* tO = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById((*it)));
								if(!tO)
								{
									assert(false && "ObjectController::_destroyOutOfRangeObjects WorldManager unable to find TangibleObject instance");
								}

								tO->removeKnownObject(player);
								player->removeKnownObject(tO);
								gMessageLib->sendDestroyObject(tO->getId(),player);
								it++;
							}
					
							hopper->removeKnownObject(player);
							player->removeKnownObject(hopper);
							
							gMessageLib->sendDestroyObject(hopper->getId(),player);					
					}

					hopper = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(factory->getOutputHopper()));
					if(hopper)
					{
							ObjectIDList*			ol = hopper->getObjects();
							ObjectIDList::iterator	it = ol->begin();

							while(it != ol->end())
							{
								TangibleObject* tO = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById((*it)));
								if(!tO)
								{
									assert(false && "ObjectController::_destroyOutOfRangeObjects WorldManager unable to find TangibleObject instance");
								}

								//PlayerObject* player = dynamic_cast<PlayerObject*>(gWorldManager->getObjectById(targetObject->getId()));
								tO->removeKnownObject(player);
								player->removeKnownObject(tO);
								gMessageLib->sendDestroyObject(tO->getId(),player);
								
								it++;
							}
					
							hopper->removeKnownObject(player);
							player->removeKnownObject(hopper);
							gMessageLib->sendDestroyObject(hopper->getId(),player);					
					}

				}
			}
			// send a destroy to us
			gMessageLib->sendDestroyObject(object->getId(),player);

			// we don't know each other anymore
			knownObjects->erase(objIt++);
			object->removeKnownObject(player);

			if (++messageCount >= objectDestroyLimit)
			{
				// gLogger->logMsg("Pausing sendDestroyObject()-calls.");
				break;
			}
			continue;
		}

		++objIt;
	}
	// For test
	bool allDestroyed = false;
	if (objIt == knownObjects->end())
	{
		// gLogger->logMsg("Finished sendDestroyObject()-calls.");
		allDestroyed = true;
	}
	return allDestroyed;
}

//=============================================================================
//
//	Update the world around the player.
//
//	NOTE (by ERU): This code need to be re-written,
//	right now it's hard to follow and very difficult to do changes without getting secondary effects not wanted...
//
//	THIS IS AN EXAMPLE OF HOW NOT TO WRITE CODE, MIXING EVERYTHING ETC....

uint64 ObjectController::playerWorldUpdate(bool forcedUpdate)
{
	PlayerObject* player = dynamic_cast<PlayerObject*>(mObject);

	// If we already are busy, don't start another update.
	// ie if this is called by the worldmanager timer because we still have unupdated objects
	// in our resultmap
	if (!(mUpdatingObjects || mDestroyOutOfRangeObjects || forcedUpdate))
	{
		// If we have been inactive for too long, let's update the world.
		if (player->getCurrentSpeed() == 0.0)
		{
			//is  this the amount of full updates already running ?
			if (++mFullUpdateTrigger >= 15)		// We only check this when we are running idle with low frequency
			{
				// gLogger->logMsg("... sitting still to long!");
				// Let's update the world
				forcedUpdate = true;
				mFullUpdateTrigger = 0;
			}
		}
		else
		{
			mFullUpdateTrigger = 0;
		}
	}

	// Are we inside or outside?
	if (player->getParentId() != 0)
	{
		// We are inside.
		if (mUpdatingObjects || forcedUpdate)
		{
			// Just entered the building?
			// if (!mUpdatingObjects)
			// We need to abort any pending operation if we get a forcedUpdate (meaning entered, changed or left a cell or subzone).
			if (forcedUpdate)
			{
				// Update all.
				// gLogger->logMsg("ObjController::handleDataTransformWithParent: _findInRangeObjectsInside(true)");
				_findInRangeObjectsInside(true);
			}
		}
		else
		{
			// This is the faster update, stil based on SI though.
			_findInRangeObjectsInside(false);
		}
		// Update some of the objects we found.
		mUpdatingObjects = !_updateInRangeObjectsInside();

	}
	else
	{
		// We are outside.
		bool OutOfUpdateRange = false;

		// If we "just stopped" and not busy with updating, make a full update.
		if (!mUpdatingObjects && !mDestroyOutOfRangeObjects)
		{
			// We are not "busy" processing anything from previous sessions.
			if (player->getCurrentSpeed() == 0.0)
			{
				if (mMovementInactivityTrigger > 0)
				{
					if (--mMovementInactivityTrigger == 0)
					{
						// gLogger->logMsg("We are not moving...");
						// We are not moving, but how far are we from last full update pos?
                        if (glm::distance(player->mPosition, player->getLastUpdatePosition()) < 16)
						{
							// Force a full update, inclusive of saving current "update pos".
							// gLogger->logMsg("... forced update!");
							OutOfUpdateRange = true;
						}
						else
						{
							// gLogger->logMsgF("... but to close to last update pos, %.1f",MSG_NORMAL, player->mPosition.distance2D(player->getLastUpdatePosition()));
						}
					}
				}
			}
			else
			{
				mMovementInactivityTrigger = 2;		// We only check this when we are running idle with slow frequency
													// Need to be standstill for this amount of seconds * 5 (or whatever time we use for slow updates) before we update.
			}
		}

		// Position check for SI-update.
        OutOfUpdateRange |= !(glm::distance(player->mPosition, player->getLastUpdatePosition()) < 64.0f);
		//OutOfUpdateRange |= !(player->mPosition.inRange2D(player->getLastUpdatePosition(),64.0f));

		// gLogger->logMsgF("Distance = %f",MSG_NORMAL, player->mPosition.distance2D(player->getLastUpdatePosition()));

		if (mUpdatingObjects || forcedUpdate || OutOfUpdateRange)
		{
			// More than 64 m from where we loaded SI, reload it.
			// We need to abort any pending operation if we get a forcedUpdate (meaning entered, changed or left a cell or subzone).
			if ((forcedUpdate) || OutOfUpdateRange)
			{
				// Save these coordinates
				// gLogger->logMsg("forcedUpdate");

				mDestroyOutOfRangeObjects = false;	// Stop the destroy-messages, in case we already have started to send them.
				if (OutOfUpdateRange)
				{
					// gLogger->logMsg("Out of 64m range");
					player->setLastUpdatePosition(player->mPosition);

					//If our player is mounted let's update his mount
					if(player->checkIfMounted() && player->getMount())
					{
						player->getMount()->setLastUpdatePosition(player->mPosition);
					}

					// We shall destroy out of range objects when we are done with the update of known objects.
					mDestroyOutOfRangeObjects = true;
				}
				_findInRangeObjectsOutside(true);
			}
		}
		else if (!mDestroyOutOfRangeObjects)
		{
			// This is the fast update, based on qt.
			// gLogger->logMsg("_findInRangeObjectsOutside(false)");

			_findInRangeObjectsOutside(false);
		}

		// Update some of the objects we found.
		mUpdatingObjects = !_updateInRangeObjectsOutside();

		if (!mUpdatingObjects)
		{
			// We are not updating new objects.
			if (mDestroyOutOfRangeObjects)
			{
				// We are ready to destroy objects out of range.
				if (_destroyOutOfRangeObjects(&mInRangeObjects))
				{
					// All objects are now destroyed.
					mDestroyOutOfRangeObjects = false;

					// If active target out of range, clear.
					if (player->getTarget())
					{
						// gLogger->logMsgF("playerWorldUpdate have a Target of type %d", MSG_NORMAL, player->getTarget()->getType());

						// The list of objects we shall check for untargeting consist of all objects that we can "interact with".
						if ((player->getTarget()->getType() & (ObjType_Player | ObjType_NPC | ObjType_Creature)) ||
							((player->getTarget()->getType() == ObjType_Tangible) && (dynamic_cast<TangibleObject*>(player->getTarget())->getTangibleGroup() == TanGroup_TicketCollector)))
						{
							if (!(player->checkKnownObjects(player->getTarget())))
							{
								player->setTarget(NULL);
								gMessageLib->sendTargetUpdateDeltasCreo6(player);
								// gLogger->logMsg("playerWorldUpdate clear Target");
							}
						}
					}
				}
			}
		}

	}

	uint64 msToWait = 4900;		// Will give 5 sec.

	if (mUpdatingObjects || mDestroyOutOfRangeObjects)
	{
		// We are busy, need to continue processing asap.
		msToWait = 900;		// This should make us tick every second, since that's the base time for the timer we use.
	}
	return msToWait;
}
