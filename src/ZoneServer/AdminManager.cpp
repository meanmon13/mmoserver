/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------

// NOTE: This file is now under re-construction, implementing the real non-persistent database interface.
// So we have to live with duplicate code for a while.

*/

#include "AdminManager.h"

#include "PlayerObject.h"
#include "WorldManager.h"
#include "ZoneOpcodes.h"

#include "MessageLib/MessageLib.h"

#include "LogManager/LogManager.h"

#include "Common/Message.h"
#include "Common/MessageDispatch.h"
#include "Utils/utils.h"

#include <cassert>


//======================================================================================================================
//
// Container for admin requests
//

class AdminRequestObject
{
	public:
		AdminRequestObject(uint64 adminRequestType, string reason, int32 timeToLive) :
			mAdminRequestType(adminRequestType),
			mReason(reason),
			mTimeToLive(timeToLive){ }

		~AdminRequestObject()
		{
			// gLogger->logMsgF("AdminRequestObject::~AdminRequestObject() invoked", MSG_NORMAL);
		}

		uint64 mAdminRequestType;
		string mReason;
		int32 mTimeToLive;

	private:
		AdminRequestObject();
};


//=============================================================================

AdminManager* AdminManager::mInstance = NULL;


//======================================================================================================================

AdminManager* AdminManager::Instance()
{
	if (!mInstance)
	{
		// mInstance = new AdminManager(gZoneServer->getDispatcher());
		assert(mInstance != NULL && "AdminManager::Init must be called prior to AdminManager::Instance calls");
	}
	return mInstance;
}
//======================================================================================================================

AdminManager* AdminManager::Init(MessageDispatch* messageDispatch)
{
	if (!mInstance)
	{
		mInstance = new AdminManager(messageDispatch);
	}
	return mInstance;
}

//======================================================================================================================
//
// This constructor prevents the default constructor to be used, as long as the constructor is keept private.
//

AdminManager::AdminManager()
{
}

//======================================================================================================================
//

AdminManager::AdminManager(MessageDispatch* messageDispatch) :
							mMessageDispatch(messageDispatch),
							mPendingShutdown(false),
							mTerminateServer(false)

{
	// gLogger->logMsgF("AdminManager::AdminManager() invoked", MSG_NORMAL);
	this->registerCallbacks();
}


//=============================================================================


//=============================================================================

AdminManager::~AdminManager()
{
	// gLogger->logMsgF("AdminManager::~AdminManager() invoked", MSG_NORMAL);
	this->unregisterCallbacks();

	AdminRequests::iterator adminRequestIterator = mAdminRequests.begin();

	while (adminRequestIterator != mAdminRequests.end())
	{
		delete ((*adminRequestIterator).second);
		adminRequestIterator++;
	}
	mAdminRequests.clear();
	mInstance = NULL;
}


//======================================================================================================================

void AdminManager::registerCallbacks(void)
{
	mMessageDispatch->registerSessionlessDispatchClient(AdminAccountId);
	mMessageDispatch->RegisterMessageCallback(opIsmScheduleShutdown,this);
	mMessageDispatch->RegisterMessageCallback(opIsmCancelShutdown,this);
}

//======================================================================================================================

void AdminManager::unregisterCallbacks(void)
{
	mMessageDispatch->UnregisterMessageCallback(opIsmScheduleShutdown);
	mMessageDispatch->UnregisterMessageCallback(opIsmCancelShutdown);
	mMessageDispatch->unregisterSessionlessDispatchClient(AdminAccountId);
}