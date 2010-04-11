/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/

#ifndef ANH_ZONESERVER_ADMIN_MANAGER_H
#define ANH_ZONESERVER_ADMIN_MANAGER_H

#include "Common/MessageDispatchCallback.h"
#include <map>
//=============================================================================

class AdminRequestObject;
class MessageDispatch;

typedef std::map<uint64, AdminRequestObject*>	AdminRequests;
typedef std::map<uint64, uint64>				AdminRequestHandlers;

enum AdminRequestt
{
	AdminScheduledShutdown		= 0,
	AdminEmergenyShutdown		= 1
};

//=============================================================================

class AdminManager : public MessageDispatchCallback
{
	public:

		static AdminManager* Instance(void);
		static AdminManager* Init(MessageDispatch* messageDispatch);

		static inline void deleteManager(void)
		{
			if (mInstance)
			{
				delete mInstance;
				mInstance = NULL;
			}
		}

		void registerCallbacks(void);
		void unregisterCallbacks(void);

		bool shutdownPending(void) { return mPendingShutdown;}
		bool shutdownZone(void) { return mTerminateServer;}

	protected:
		// AdminManager(AdminRequestObject* adminObject);
		// AdminManager();
		AdminManager(MessageDispatch* messageDispatch);
		~AdminManager();

	private:
		// This constructor prevents the default constructor to be used, since it is private.
		AdminManager();

		static AdminManager* mInstance;
		AdminRequests mAdminRequests;
		MessageDispatch* mMessageDispatch;
		bool	mPendingShutdown;
		bool	mTerminateServer;

		
};

//=============================================================================



#endif

