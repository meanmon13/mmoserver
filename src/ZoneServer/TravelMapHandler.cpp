/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/

#include "TravelMapHandler.h"
#include "Inventory.h"
#include "ObjectFactory.h"
#include "PlayerObject.h"
#include "Shuttle.h"
#include "TicketCollector.h"
#include "TravelTerminal.h"
#include "TravelTicket.h"
#include "UIManager.h"
#include "UITicketSelectListBox.h"
#include "WorldManager.h"
#include "ZoneOpcodes.h"

#include "MessageLib/MessageLib.h"
#include "LogManager/LogManager.h"

#include "Common/DispatchClient.h"
#include "Common/Message.h"
#include "Common/MessageDispatch.h"
#include "Common/MessageFactory.h"
#include "Common/MessageOpcodes.h"

#include "DatabaseManager/Database.h"
#include "DatabaseManager/DatabaseResult.h"
#include "DatabaseManager/DataBinding.h"

#include "Utils/utils.h"
#include "Utils/rand.h"

bool				TravelMapHandler::mInsFlag    = false;
TravelMapHandler*	TravelMapHandler::mSingleton  = NULL;

//======================================================================================================================

TravelMapHandler::TravelMapHandler(Database* database, MessageDispatch* dispatch,uint32 zoneId)
	: mDBAsyncPool(sizeof(TravelMapAsyncContainer))
  , mDatabase(database)
  , mMessageDispatch(dispatch)
  , mPointCount(0)
  , mRouteCount(0)
  , mZoneId(zoneId)
  , mCellPointsLoaded(false)
  , mRoutesLoaded(false)
  , mWorldPointsLoaded(false)

{
	mMessageDispatch->RegisterMessageCallback(opPlanetTravelPointListRequest,this);

	// load our points in world
	mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.malloc()) TravelMapAsyncContainer(TMQuery_PointsInWorld),
													"SELECT DISTINCT(terminals.dataStr),terminals.x,terminals.y,terminals.z,terminals.dataInt1,"
													"terminals.dataInt2,terminals.planet_id,"
													"spawn_shuttle.X,spawn_shuttle.Y,spawn_shuttle.Z"
													" FROM terminals"
													" INNER JOIN spawn_shuttle ON (terminals.dataInt3 = spawn_shuttle.id)"
													" WHERE terminals.terminal_type = 16 AND"
													" terminals.parent_id = 0"
													" GROUP BY terminals.dataStr");

	// load travel points in cells
	mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.malloc()) TravelMapAsyncContainer(TMQuery_PointsInCells),
													"SELECT DISTINCT(terminals.dataStr),terminals.planet_id,terminals.dataInt1,terminals.dataInt2,"
													"buildings.x,buildings.y,buildings.z,spawn_shuttle.X,spawn_shuttle.Y,spawn_shuttle.Z"
													" FROM terminals"
													" INNER JOIN spawn_shuttle ON (terminals.dataInt3 = spawn_shuttle.id)"
													" INNER JOIN cells ON (terminals.parent_id = cells.id)"
													" INNER JOIN buildings ON (cells.parent_id = buildings.id)"
													" WHERE"
													" (terminals.terminal_type = 16) AND"
													" (terminals.parent_id <> 0)"
													" GROUP BY terminals.dataStr");
	// load planet routes and base prices
	mDatabase->ExecuteSqlAsync(this,new(mDBAsyncPool.malloc()) TravelMapAsyncContainer(TMQuery_PlanetRoutes),"SELECT * FROM travel_planet_routes");
}


//======================================================================================================================

TravelMapHandler::~TravelMapHandler()
{
	mInsFlag = false;
	delete(mSingleton);
}
//======================================================================================================================

TravelMapHandler*	TravelMapHandler::Init(Database* database, MessageDispatch* dispatch,uint32 zoneId)
{
	if(!mInsFlag)
	{
		mSingleton = new TravelMapHandler(database,dispatch,zoneId);
		mInsFlag = true;
		return mSingleton;
	}
	else
		return mSingleton;
}

//======================================================================================================================

void TravelMapHandler::Shutdown()
{
	mMessageDispatch->UnregisterMessageCallback(opPlanetTravelPointListRequest);

	for(uint8 i = 0;i < 50;i++)
	{
		TravelPointList::iterator it = mTravelPoints[i].begin();
		while(it != mTravelPoints[i].end())
		{
			delete(*it);
			mTravelPoints[i].erase(it);
			it = mTravelPoints[i].begin();
		}
	}
}

//======================================================================================================================

void TravelMapHandler::handleDispatchMessage(uint32 opcode, Message* message, DispatchClient* client)
{
	switch(opcode)
	{
		case opPlanetTravelPointListRequest:
		{
			_processTravelPointListRequest(message,client);
		}
		break;

		default:
			gLogger->logMsgF("TravelMapHandler::handleDispatchMessage: Unhandled opcode %u",MSG_NORMAL,opcode);
		break;
	}
}

//=======================================================================================================================
void TravelMapHandler::handleDatabaseJobComplete(void* ref,DatabaseResult* result)
{
	TravelMapAsyncContainer* asynContainer = reinterpret_cast<TravelMapAsyncContainer*>(ref);
	switch(asynContainer->mQueryType)
	{
		case TMQuery_PointsInWorld:
		{
			DataBinding* binding = mDatabase->CreateDataBinding(10);
			binding->addField(DFT_string,offsetof(TravelPoint,descriptor),64,0);
			binding->addField(DFT_float,offsetof(TravelPoint,x),4,1);
			binding->addField(DFT_float,offsetof(TravelPoint,y),4,2);
			binding->addField(DFT_float,offsetof(TravelPoint,z),4,3);
			binding->addField(DFT_uint8,offsetof(TravelPoint,portType),1,4);
			binding->addField(DFT_uint32,offsetof(TravelPoint,taxes),4,5);
			binding->addField(DFT_uint16,offsetof(TravelPoint,planetId),2,6);
			binding->addField(DFT_float,offsetof(TravelPoint,spawnX),4,7);
			binding->addField(DFT_float,offsetof(TravelPoint,spawnY),4,8);
			binding->addField(DFT_float,offsetof(TravelPoint,spawnZ),4,9);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				TravelPoint* travelPoint = new TravelPoint();
				result->GetNextRow(binding,travelPoint);

				mTravelPoints[travelPoint->planetId].push_back(travelPoint);
			}

			mPointCount += static_cast<uint32>(count);
			mWorldPointsLoaded = true;

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case TMQuery_PointsInCells:
		{
			DataBinding* binding = mDatabase->CreateDataBinding(10);
			binding->addField(DFT_string,offsetof(TravelPoint,descriptor),64,0);
			binding->addField(DFT_uint16,offsetof(TravelPoint,planetId),2,1);
			binding->addField(DFT_uint8,offsetof(TravelPoint,portType),1,2);
			binding->addField(DFT_uint32,offsetof(TravelPoint,taxes),4,3);
			binding->addField(DFT_float,offsetof(TravelPoint,x),4,4);
			binding->addField(DFT_float,offsetof(TravelPoint,y),4,5);
			binding->addField(DFT_float,offsetof(TravelPoint,z),4,6);
			binding->addField(DFT_float,offsetof(TravelPoint,spawnX),4,7);
			binding->addField(DFT_float,offsetof(TravelPoint,spawnY),4,8);
			binding->addField(DFT_float,offsetof(TravelPoint,spawnZ),4,9);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				TravelPoint* travelPoint = new TravelPoint();
				result->GetNextRow(binding,travelPoint);

				mTravelPoints[travelPoint->planetId].push_back(travelPoint);
			}

			mPointCount += static_cast<uint32>(count);
			mCellPointsLoaded = true;

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		case TMQuery_PlanetRoutes:
		{

			TravelRoute route;

			DataBinding* binding = mDatabase->CreateDataBinding(3);
			binding->addField(DFT_uint16,offsetof(TravelRoute,srcId),2,0);
			binding->addField(DFT_uint16,offsetof(TravelRoute,destId),2,1);
			binding->addField(DFT_int32,offsetof(TravelRoute,price),4,2);

			uint64 count = result->getRowCount();

			for(uint64 i = 0;i < count;i++)
			{
				result->GetNextRow(binding,&route);
				mTravelRoutes[route.srcId].push_back(std::make_pair(route.destId,route.price));
			}

			mRouteCount = static_cast<uint32>(count);
			mRoutesLoaded = true;

			mDatabase->DestroyDataBinding(binding);
		}
		break;

		default:break;
	}

	mDBAsyncPool.free(asynContainer);

	if(mWorldPointsLoaded && mCellPointsLoaded && mRoutesLoaded)
	{
		mWorldPointsLoaded = false;
		mCellPointsLoaded = false;
		mRoutesLoaded = false;

		if(result->getRowCount())
			gLogger->logMsgLoadSuccess("TravelMapHandler::Loading %u TravelRoutes and %u TravelPoints...",MSG_NORMAL,mPointCount,mRouteCount);
		else
			gLogger->logMsgLoadFailure("TravelMapHandler::Loading Travel Routes/Points...",MSG_NORMAL);

	}
}

//=======================================================================================================================

void TravelMapHandler::_processTravelPointListRequest(Message* message,DispatchClient* client)
{
	PlayerObject* playerObject = dynamic_cast<PlayerObject*>(gWorldManager->getObjectById(message->getUint64()));

	if(playerObject != NULL && playerObject->isConnected())
	{
		bool excludeQueryPosition = false;

		// we need to know where we query from
		TravelTerminal* terminal = playerObject->getTravelPoint();

		if(terminal == NULL)
		{
			gLogger->logMsgF("TravelMapHandler::_processTravelListRequest: No TravelPosition set, player %"PRIu64"",MSG_NORMAL,playerObject->getId());
			return;
		}

		string requestedPlanet;
		message->getStringAnsi(requestedPlanet);

		// find our planetId
		uint8 planetId = 0;

		while((strcmp(requestedPlanet.getAnsi(),gWorldManager->getPlanetNameById(planetId)) != 0))
			planetId++;

		// see if we need to skip a point
		uint32 pointCount = 0;
		char	queryPoint[64];
		TravelPoint* qP = NULL;

		// exclude current position
		if(mZoneId == planetId)
		{
			excludeQueryPosition = true;
			pointCount = mTravelPoints[planetId].size()-1;
		}
		// exclude shuttleports from other planets
		else
		{
			TravelPointList::iterator it = mTravelPoints[planetId].begin();
			while(it != mTravelPoints[planetId].end())
			{
				if((*it)->portType == 1)
					pointCount++;

				++it;
			}
		}

		// get our query point
		strcpy(queryPoint,(playerObject->getTravelPoint())->getPosDescriptor().getAnsi());

		TravelPointList::iterator it = mTravelPoints[mZoneId].begin();
		while(it != mTravelPoints[mZoneId].end())
		{
			TravelPoint* tp = (*it);

			if(strcmp(queryPoint,tp->descriptor) == 0)
			{
				qP = tp;
				break;
			}
			++it;
		}

		// build our response
		gMessageFactory->StartMessage();
		gMessageFactory->addUint32(opPlanetTravelPointListResponse);
		gMessageFactory->addString(requestedPlanet);

		// point descriptors
		gMessageFactory->addUint32(pointCount);

		it = mTravelPoints[planetId].begin();
		while(it != mTravelPoints[planetId].end())
		{
			TravelPoint* tp = (*it);
			string desc = tp->descriptor;

			if(excludeQueryPosition)
			{
				if(qP == tp)
				{
					++it;
					continue;
				}
				else
					gMessageFactory->addString(desc);
			}
			else
			{
				if(tp->portType == 1)
					gMessageFactory->addString(desc);
			}

			++it;
		}

		// positions
		gMessageFactory->addUint32(pointCount);

		it = mTravelPoints[planetId].begin();
		while(it != mTravelPoints[planetId].end())
		{
			TravelPoint* tp = (*it);

			if(excludeQueryPosition)
			{
				if(qP == tp)
				{
					++it;
					continue;
				}
				else
				{
					gMessageFactory->addFloat(tp->x);
					gMessageFactory->addFloat(tp->y);
					gMessageFactory->addFloat(tp->z);
				}
			}
			else
			{
				if(tp->portType == 1)
				{
					gMessageFactory->addFloat(tp->x);
					gMessageFactory->addFloat(tp->y);
					gMessageFactory->addFloat(tp->z);
				}
			}
			++it;
		}

		// taxes
		gMessageFactory->addUint32(pointCount);

		it = mTravelPoints[planetId].begin();
		while(it != mTravelPoints[planetId].end())
		{
			TravelPoint* tp = (*it);

			if(excludeQueryPosition)
			{
				if(qP == tp)
				{
					++it;
					continue;
				}
				else
					gMessageFactory->addUint32(tp->taxes);
			}
			else
			{
				if(tp->portType == 1)
					gMessageFactory->addUint32(tp->taxes);
			}
			++it;
		}

		// reachable ?
		gMessageFactory->addUint32(pointCount);

		it = mTravelPoints[planetId].begin();
		while(it != mTravelPoints[planetId].end())
		{
			TravelPoint* tp = (*it);

			if(excludeQueryPosition)
			{
				if(qP == tp)
				{
					++it;
					continue;
				}
				// we on this planet so all reachable
				else
					gMessageFactory->addUint8(1);
			}
			else
			{
				if(qP->portType == 1)
					gMessageFactory->addUint8(1);
				else
					gMessageFactory->addUint8(0);
			}
			++it;
		}

		Message* newMessage = gMessageFactory->EndMessage();

		client->SendChannelA(newMessage, playerObject->getAccountId(),  CR_Client, 6);
	}
	else
		gLogger->logMsgF("TravelMapHandler::_processTravelListRequest: Couldnt find player for %u",MSG_NORMAL,client->getAccountId());
}

//=======================================================================================================================

void TravelMapHandler::getTicketInformation(BStringVector vQuery,TicketProperties* ticketProperties)
{
	ticketProperties->srcPlanetId = 0;
	while((strcmp(vQuery[0].getAnsi(),gWorldManager->getPlanetNameById(static_cast<uint8>(ticketProperties->srcPlanetId))) != 0))
		ticketProperties->srcPlanetId++;

	ticketProperties->dstPlanetId = 0;
	while((strcmp(vQuery[2].getAnsi(),gWorldManager->getPlanetNameById(static_cast<uint8>(ticketProperties->dstPlanetId))) != 0))
		ticketProperties->dstPlanetId++;

	TravelRoutes::iterator it = mTravelRoutes[ticketProperties->srcPlanetId].begin();
	while(it != mTravelRoutes[ticketProperties->srcPlanetId].end())
	{
		if((*it).first == ticketProperties->dstPlanetId)
		{
			ticketProperties->price = (*it).second;
			break;
		}
		++it;
	}

	TravelPointList::iterator tpIt = mTravelPoints[ticketProperties->srcPlanetId].begin();
	while(tpIt != mTravelPoints[ticketProperties->srcPlanetId].end())
	{
		TravelPoint* tp = (*tpIt);
		string desc = tp->descriptor;

		if(strcmp(strRep(std::string(vQuery[1].getAnsi()),"_"," ").c_str(),desc.getAnsi()) == 0)
		{
			ticketProperties->srcPoint = tp;
			break;
		}

		++tpIt;
	}

	ticketProperties->dstPoint = NULL;

	tpIt = mTravelPoints[ticketProperties->dstPlanetId].begin();
	while(tpIt != mTravelPoints[ticketProperties->dstPlanetId].end())
	{
		//tp = (*tpIt);

		string desc = (*tpIt)->descriptor;

		if(strcmp(strRep(std::string(vQuery[3].getAnsi()),"_"," ").c_str(),desc.getAnsi()) == 0)
		{
			ticketProperties->dstPoint = (*tpIt);
			ticketProperties->price += (*tpIt)->taxes;

			break;
		}
		++tpIt;
	}
}

//=======================================================================================================================

TravelPoint* TravelMapHandler::getTravelPoint(uint16 planetId,string name)
{
	TravelPointList::iterator it = mTravelPoints[planetId].begin();
	while(it != mTravelPoints[planetId].end())
	{
		if(strcmp(name.getAnsi(),(*it)->descriptor) == 0)
			return(*it);

		++it;
	}
	return(NULL);
}

//=======================================================================================================================

bool TravelMapHandler::findTicket(PlayerObject* player, string port)
{
	uint32	zoneId = gWorldManager->getZoneId();

	ObjectIDList* invObjects = dynamic_cast<Inventory*>(player->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory))->getObjects();
	ObjectIDList::iterator it = invObjects->begin();

	while(it != invObjects->end())
	{
		TravelTicket* ticket = dynamic_cast<TravelTicket*>(gWorldManager->getObjectById((*it)));
		if(ticket)
		{
			string srcPoint		= (int8*)((ticket->getAttribute<std::string>("travel_departure_point")).c_str());
			uint16 srcPlanetId	= static_cast<uint16>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_departure_planet")).c_str())));

			// see if we got at least 1
			if(srcPlanetId == zoneId && strcmp(srcPoint.getAnsi(),port.getAnsi()) == 0)
			{
				return false;
			}
		}
		++it;
	}
	return true;;
}

//=======================================================================================================================

void TravelMapHandler::createTicketSelectMenu(PlayerObject* playerObject, Shuttle* shuttle, string port)
{
	BStringVector	availableTickets;
	uint32			zoneId = gWorldManager->getZoneId();

	ObjectIDList*			invObjects	= dynamic_cast<Inventory*>(playerObject->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory))->getObjects();
	ObjectIDList::iterator	it			= invObjects->begin();

	while(it != invObjects->end())
	{
		TravelTicket* ticket = dynamic_cast<TravelTicket*>(gWorldManager->getObjectById((*it)));
		if(ticket)
		{
			string srcPoint		= (int8*)((ticket->getAttribute<std::string>("travel_departure_point")).c_str());
			uint16 srcPlanetId	= static_cast<uint8>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_departure_planet")).c_str())));

			if(srcPlanetId == zoneId && strcmp(srcPoint.getAnsi(),port.getAnsi()) == 0)
			{
				string dstPoint = (int8*)((ticket->getAttribute<std::string>("travel_arrival_point")).c_str());

				availableTickets.push_back(dstPoint.getAnsi());
			}
		}
		++it;
	}

	gUIManager->createNewTicketSelectListBox(this,"handleTicketSelect","select destination","Select destination",availableTickets,playerObject,port,shuttle);
}

//=======================================================================================================================

void TravelMapHandler::handleUIEvent(uint32 action,int32 element,string inputStr,UIWindow* window)
{
	if(!action && element != -1 )
	{
		uint32					zoneId			= gWorldManager->getZoneId();
		PlayerObject*			playerObject	= window->getOwner();
		UITicketSelectListBox*	listBox			= dynamic_cast<UITicketSelectListBox*>(window);

		if(playerObject->getSurveyState() || playerObject->getSamplingState() || !listBox)
			return;

		Shuttle* shuttle = listBox->getShuttle();

		if(!shuttle)
		{
			return;
		}

		if (!shuttle->availableInPort())
		{
			gMessageLib->sendSystemMessage(playerObject,L"","travel","shuttle_not_available");
			return;
		}

        if((playerObject->getParentId() != shuttle->getParentId()) || (glm::distance(playerObject->mPosition, shuttle->mPosition) > 25.0f))
		{
			gMessageLib->sendSystemMessage(playerObject,L"","travel","boarding_too_far");

			return;
		}

		ObjectIDList*			invObjects		= dynamic_cast<Inventory*>(playerObject->getEquipManager()->getEquippedObject(CreatureEquipSlot_Inventory))->getObjects();
		ObjectIDList::iterator	it				= invObjects->begin();

		while(it != invObjects->end())
		{
			TravelTicket* ticket = dynamic_cast<TravelTicket*>(gWorldManager->getObjectById((*it)));
			if(ticket)
			{
				string srcPoint		= (int8*)((ticket->getAttribute<std::string>("travel_departure_point")).c_str());
				string dstPointStr	= (int8*)((ticket->getAttribute<std::string>("travel_arrival_point")).c_str());
				uint16 srcPlanetId	= static_cast<uint8>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_departure_planet")).c_str())));
				uint16 dstPlanetId	= static_cast<uint8>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_arrival_planet")).c_str())));

				BStringVector* items = (dynamic_cast<UIListBox*>(window))->getDataItems();
				string selectedDst = items->at(element);
				selectedDst.convert(BSTRType_ANSI);

				if(srcPlanetId == zoneId && (strcmp(dstPointStr.getAnsi(),selectedDst.getAnsi()) == 0)&&(strcmp(srcPoint.getAnsi(),listBox->getPort().getAnsi()) == 0))
				{
					TravelPoint* dstPoint = gTravelMapHandler->getTravelPoint(dstPlanetId,dstPointStr);

					if(dstPoint != NULL)
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
							gMessageLib->sendClusterZoneTransferRequestByTicket(playerObject,ticket->getId(), dstPoint->planetId);
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

//=======================================================================================================================

void TravelMapHandler::useTicket(PlayerObject* playerObject, TravelTicket* ticket,Shuttle* shuttle)
{
	uint32	zoneId = gWorldManager->getZoneId();

	// in range check
	if(playerObject->getParentId() !=  shuttle->getParentId())
	{
		gMessageLib->sendSystemMessage(playerObject,L"","travel","shuttle_not_available");
		return;
	}

	TicketCollector* collector = dynamic_cast<TicketCollector*>(gWorldManager->getObjectById(shuttle->getCollectorId()));
	string port = collector->getPortDescriptor();

	if(port.getCrc() == BString("Theed Starport").getCrc())
	{
		shuttle->setShuttleState(ShuttleState_InPort);
	}

	if (!shuttle->availableInPort())
	{
		gMessageLib->sendSystemMessage(playerObject,L"","travel","shuttle_not_available");
		return;
	}

	string srcPoint		= (int8*)((ticket->getAttribute<std::string>("travel_departure_point")).c_str());
	uint16 srcPlanetId	= static_cast<uint8>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_departure_planet")).c_str())));
	uint16 dstPlanetId	= static_cast<uint8>(gWorldManager->getPlanetIdByName((int8*)((ticket->getAttribute<std::string>("travel_arrival_planet")).c_str())));
	string dstPointStr	= (int8*)((ticket->getAttribute<std::string>("travel_arrival_point")).c_str());

	// see if we are at the right location
	if(srcPlanetId != zoneId || strcmp(srcPoint.getAnsi(),port.getAnsi()) != 0)
	{
		gMessageLib->sendSystemMessage(playerObject,L"","travel","wrong_shuttle");
		return;
	}

	//ok lets travel
	if(TravelPoint* dstPoint = gTravelMapHandler->getTravelPoint(dstPlanetId,dstPointStr))
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
			ticket = NULL;
			gWorldManager->warpPlanet(playerObject,destination,0);
		}
		else
		{
			gMessageLib->sendClusterZoneTransferRequestByTicket(playerObject,ticket->getId(), dstPoint->planetId);
		}
		return;
	}
	else
	{
		gLogger->logMsg("TicketCollector: Error getting TravelPoint");
	}
}

//=======================================================================================================================



