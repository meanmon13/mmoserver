/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/
#include "TicketCollector.h"
#include "Inventory.h"
#include "PlayerObject.h"
#include "ObjectFactory.h"
#include "Shuttle.h"
#include "TravelMapHandler.h"
#include "TravelTicket.h"
#include "UIListBox.h"
#include "UIManager.h"
#include "WorldManager.h"
#include "MessageLib/MessageLib.h"
#include "LogManager/LogManager.h"

#include <time.h>
#include "utils/rand.h"

//=============================================================================

TicketCollector::TicketCollector() : TangibleObject(),
mShuttle(NULL)
{
	mTanGroup	= TanGroup_TicketCollector;
	mTanType	= TanType_TicketCollector;

	mPortDescriptor.setLength(64);

}

//=============================================================================

TicketCollector::~TicketCollector()
{
}

//=============================================================================

void TicketCollector::handleObjectMenuSelect(uint8 messageType,Object* srcObject)
{
	if(messageType == radId_itemUse)
	{
		PlayerObject* playerObject = dynamic_cast<PlayerObject*>(srcObject);
		
		if(!playerObject)
		{
			gLogger->logMsgF("TicketCollector: no player",MSG_HIGH);
			return;
		}

		if(!mShuttle)
		{
			gLogger->logMsgF("TicketCollector: no shuttle",MSG_HIGH);
			return;
		}

		// don't use while incapped or dead or in combat
		if(playerObject->isIncapacitated() || playerObject->isDead() || playerObject->checkState(CreatureState_Combat))
		{
			return;
		}

		// in range check for shuttle not for the droid
        if(playerObject->getParentId() != mParentId || (glm::distance(playerObject->mPosition, mShuttle->mPosition) > 25.0f))
		{
			gMessageLib->sendSystemMessage(playerObject,L"","travel","boarding_too_far");
			return;
		}

		if(playerObject->checkUIWindow("handleTicketSelect"))
		{
			gMessageLib->sendSystemMessage(playerObject,L"","travel","boarding_ticket_selection");
			return;
		}

		if(mShuttle)
		{
			  mShuttle->useShuttle(playerObject);
		}
	}
	else
		gLogger->logMsgF("TravelTerminal: Unhandled MenuSelect: %u",MSG_HIGH,messageType);
}

//=============================================================================

void TicketCollector::prepareCustomRadialMenu(CreatureObject* creatureObject, uint8 itemCount)
{
	mRadialMenu = RadialMenuPtr(new RadialMenu());
	
	mRadialMenu->addItem(1,0,radId_itemUse,radAction_ObjCallback);
	mRadialMenu->addItem(2,0,radId_examine,radAction_Default);
}


//=============================================================================

void TicketCollector::_createTicketSelectMenu(PlayerObject* playerObject)
{
	BStringVector	availableTickets;
	uint32			zoneId = gWorldManager->getZoneId();

	ObjectIDList*			invObjects	= dynamic_cast<Inventory*>(playerObject->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory))->getObjects();
	ObjectIDList::iterator	it			= invObjects->begin();

	while(it != invObjects->end())
	{
		if(TravelTicket* ticket = dynamic_cast<TravelTicket*>(gWorldManager->getObjectById((*it))))
		{
			string srcPoint		= (int8*)((ticket->getAttribute<std::string>("travel_departure_point")).c_str());
			uint16 srcPlanetId	= static_cast<uint16>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_departure_planet")).c_str())));

			if(srcPlanetId == zoneId && strcmp(srcPoint.getAnsi(),mPortDescriptor.getAnsi()) == 0)
			{
				string dstPoint = (int8*)((ticket->getAttribute<std::string>("travel_arrival_point")).c_str());

				availableTickets.push_back(dstPoint.getAnsi());
			}
		}
		++it;
	}

	gUIManager->createNewListBox(this,"handleticketselect","select destination","Select destination",availableTickets,playerObject);
}

//=============================================================================

void TicketCollector::handleUIEvent(uint32 action,int32 element,string inputStr,UIWindow* window)
{
	if (!action && element != -1 && mShuttle != NULL && mShuttle->availableInPort())
	{
		uint32			zoneId			= gWorldManager->getZoneId();
		PlayerObject*	playerObject	= window->getOwner();

		if(playerObject->getSurveyState() || playerObject->getSamplingState() || playerObject->isIncapacitated() || playerObject->isDead())
		{
			return;
		}

		// in range check
        if(playerObject->getParentId() != mParentId || (glm::distance(playerObject->mPosition, this->mPosition) > 10.0f))
		{
			gMessageLib->sendSystemMessage(playerObject,L"","travel","boarding_too_far");
			return;
		}

		ObjectIDList*			invObjects	= dynamic_cast<Inventory*>(playerObject->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory))->getObjects();
		ObjectIDList::iterator	it			= invObjects->begin();

		while(it != invObjects->end())
		{
			if(TravelTicket* ticket = dynamic_cast<TravelTicket*>(gWorldManager->getObjectById((*it))))
			{
				string srcPoint		= (int8*)((ticket->getAttribute<std::string>("travel_departure_point")).c_str());
				string dstPointStr	= (int8*)((ticket->getAttribute<std::string>("travel_arrival_point")).c_str());
				uint16 srcPlanetId	= static_cast<uint16>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_departure_planet")).c_str())));
				uint16 dstPlanetId	= static_cast<uint16>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_arrival_planet")).c_str())));

				BStringVector* items = (dynamic_cast<UIListBox*>(window))->getDataItems();
				string selectedDst = items->at(element);
				selectedDst.convert(BSTRType_ANSI);

				if(srcPlanetId == zoneId && (strcmp(srcPoint.getAnsi(),mPortDescriptor.getAnsi()) == 0)
				&& (strcmp(dstPointStr.getAnsi(),selectedDst.getAnsi()) == 0))
				{
					if(TravelPoint* dstPoint = gTravelMapHandler->getTravelPoint(dstPlanetId,dstPointStr))
					{
                        glm::vec3 destination;
						// getRand(5) return 0-4, then sub 2, and you get equal of random values at both sides of zero. (-2, -1, 0, 1, 2)
						destination.x = dstPoint->spawnX + (gRandom->getRand()%5 - 2);
						destination.y = dstPoint->spawnY;
						destination.z = dstPoint->spawnZ + (gRandom->getRand()%5 - 2);

						// If it's on this planet, then just warp, otherwize zone
						if(dstPlanetId == zoneId)
						{
							// only delete the ticket if we are warping on this planet.
							gMessageLib->sendDestroyObject(ticket->getId(),playerObject);
							gObjectFactory->deleteObjectFromDB(ticket);
							dynamic_cast<Inventory*>(playerObject->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory))->deleteObject(ticket);

							gWorldManager->warpPlanet(playerObject,destination,0);
						}
						else
						{
							gMessageLib->sendClusterZoneTransferRequestByTicket(playerObject, ticket->getId(), dstPoint->planetId);
							
						}
					}
					else
					{
						gLogger->logMsg("TicketCollector: Error getting TravelPoint");
					}
					break;
				}
			}

			++it;
		}
	}
}

//=============================================================================

void TicketCollector::travelRequest(TravelTicket* ticket,PlayerObject* playerObject)
{
	if(playerObject->getSurveyState() || playerObject->getSamplingState() || playerObject->isIncapacitated() || playerObject->isDead())
	{
		return;
	}

	uint32 zoneId		= gWorldManager->getZoneId();
	string srcPoint		= (int8*)((ticket->getAttribute<std::string>("travel_departure_point")).c_str());
	string dstPointStr	= (int8*)((ticket->getAttribute<std::string>("travel_arrival_point")).c_str());
	uint16 srcPlanetId	= static_cast<uint16>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_departure_planet")).c_str())));
	uint16 dstPlanetId	= static_cast<uint16>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_arrival_planet")).c_str())));

	if (mShuttle && mShuttle->availableInPort())
	{
		if(srcPlanetId == zoneId && (strcmp(srcPoint.getAnsi(),mPortDescriptor.getAnsi()) == 0))
		{
			TravelPoint* dstPoint = gTravelMapHandler->getTravelPoint(dstPlanetId,dstPointStr);

			if(dstPoint)
			{
                glm::vec3 destination;
				destination.x = dstPoint->spawnX + (gRandom->getRand()%5 - 2);
				destination.y = dstPoint->spawnY;
				destination.z = dstPoint->spawnZ + (gRandom->getRand()%5 - 2);

				// If it's on this planet, then just warp, otherwize zone
				if(dstPlanetId == zoneId)
				{
					// only delete the ticket if we are warping on this planet.
					gMessageLib->sendDestroyObject(ticket->getId(),playerObject);
					gObjectFactory->deleteObjectFromDB(ticket);

					dynamic_cast<Inventory*>(playerObject->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory))->deleteObject(ticket);

					gWorldManager->warpPlanet(playerObject,destination,0);
				}
				else
				{
					gMessageLib->sendClusterZoneTransferRequestByTicket(playerObject,ticket->getId(),dstPoint->planetId);
				}
			}
			else
			{
				gLogger->logMsg("TicketCollector: Error getting TravelPoint\n");
			}
		}
	}
}

//=============================================================================


