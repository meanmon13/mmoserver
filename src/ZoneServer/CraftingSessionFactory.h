/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/

#ifndef ANH_ZONESERVER_CRAFTINGSESSION_FACTORY_H
#define ANH_ZONESERVER_CRAFTINGSESSION_FACTORY_H

#include "Utils/typedefs.h"
#include <boost/pool/pool.hpp>


//=============================================================================

class CraftingSession;
class CraftingStation;
class CraftingTool;
class Database;
class PlayerObject;

namespace Anh_Utils
{
    class Clock;
}

//=============================================================================

class CraftingSessionFactory
{
	public:
		CraftingSessionFactory(Database* database);
		~CraftingSessionFactory();

		CraftingSession*		createSession(Anh_Utils::Clock* clock,PlayerObject* player,CraftingTool* tool,CraftingStation* station,uint32 expFlag);
		void					destroySession(CraftingSession* session);

	private:
		Database*						mDatabase;

		boost::pool<boost::default_user_allocator_malloc_free>	mSessionPool;
};

//=============================================================================

#endif


