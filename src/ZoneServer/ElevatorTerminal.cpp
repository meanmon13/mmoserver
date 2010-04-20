/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/
#include "ElevatorTerminal.h"
#include "CellObject.h"
#include "PlayerObject.h"
#include "ZoneServer/WorldManager.h"
#include "MessageLib/MessageLib.h"
#include "LogManager/LogManager.h"


//=============================================================================

ElevatorTerminal::ElevatorTerminal() : Terminal ()
{
	
}

//=============================================================================

ElevatorTerminal::~ElevatorTerminal()
{
}

//=============================================================================

void ElevatorTerminal::handleObjectMenuSelect(uint8 messageType,Object* srcObject)
{
	PlayerObject* playerObject = dynamic_cast<PlayerObject*>(srcObject);

	if(!playerObject || !playerObject->isConnected() || playerObject->getSamplingState() || playerObject->isIncapacitated() || playerObject->isDead())
	{
		return;
	}

	if(messageType == radId_elevatorUp)
	{
		gMessageLib->sendPlayClientEffectObjectMessage(gWorldManager->getClientEffect(mEffectUp),"",playerObject);

		// remove player from current position, elevators can only be inside
		CellObject* cell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(playerObject->getParentId()));

		if(cell)
		{
			cell->removeObject(playerObject);
		}
		else
		{
			gLogger->logMsgF("could not find cell %"PRIu64"",MSG_HIGH,playerObject->getParentId());
		}

		// put him into new one
		playerObject->mDirection = mDstDirUp;
		playerObject->mPosition  = mDstPosUp;
		playerObject->setParentId(mDstCellUp);

		cell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(mDstCellUp));

		if(cell)
		{
			cell->addObjectSecure(playerObject);
		}
		else
		{
			gLogger->logMsgF("could not find cell %"PRIu64"",MSG_HIGH,mDstCellUp);
		}

		gMessageLib->sendDataTransformWithParent(playerObject);

	}
	else if(messageType == radId_elevatorDown)
	{
		gMessageLib->sendPlayClientEffectObjectMessage(gWorldManager->getClientEffect(mEffectDown),"",playerObject);
	
		// remove player from current position, elevators can only be inside
		CellObject* cell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(playerObject->getParentId()));

		if(cell)
		{
			cell->removeObject(playerObject);
		}
		else
		{
			gLogger->logMsgF("could not find cell %"PRIu64"",MSG_HIGH,playerObject->getParentId());
		}

		// put him into new one
		playerObject->mDirection = mDstDirDown;
		playerObject->mPosition  = mDstPosDown;
		playerObject->setParentId(mDstCellDown);
		
		cell = dynamic_cast<CellObject*>(gWorldManager->getObjectById(mDstCellDown));

		if(cell)
		{
			cell->addObjectSecure(playerObject);
		}
		else
		{
			gLogger->logMsgF("could not find cell %"PRIu64"",MSG_HIGH,mDstCellDown);
		}

		gMessageLib->sendDataTransformWithParent(playerObject);
	}
	else
	{
		gLogger->logMsgF("ElevatorTerminal: Unhandled MenuSelect: %u",MSG_HIGH,messageType);
	}
}

//=============================================================================

void ElevatorTerminal::prepareCustomRadialMenu(CreatureObject* creatureObject, uint8 itemCount)
{
	RadialMenu* radial = new RadialMenu();
	
	if(mTanType == TanType_ElevatorUpTerminal)
	{
		radial->addItem(1,0,radId_examine,radAction_Default);
		radial->addItem(2,0,radId_elevatorUp,radAction_ObjCallback,"@elevator_text:up");
	}
	else if(mTanType == TanType_ElevatorDownTerminal)
	{
		radial->addItem(1,0,radId_examine,radAction_Default);
		radial->addItem(2,0,radId_elevatorDown,radAction_ObjCallback,"@elevator_text:down");
	}
	else
	{
		radial->addItem(1,0,radId_examine,radAction_Default);
		radial->addItem(2,0,radId_elevatorUp,radAction_ObjCallback,"@elevator_text:up");
		radial->addItem(3,0,radId_elevatorDown,radAction_ObjCallback,"@elevator_text:down");
	}

	mRadialMenu = RadialMenuPtr(radial);
}

//=============================================================================

