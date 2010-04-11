/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/
#include "CraftingSessionFactory.h"
#include "CraftingSession.h"
#include "PlayerObject.h"
#include "LogManager/LogManager.h"
#include "DatabaseManager/Database.h"

#include "Utils/utils.h"

//=============================================================================

CraftingSessionFactory::CraftingSessionFactory(Database* database) :
mDatabase(database),
mSessionPool(sizeof(CraftingSession))
{
}

//=============================================================================

CraftingSessionFactory::~CraftingSessionFactory()
{
}

//=============================================================================

CraftingSession* CraftingSessionFactory::createSession(Anh_Utils::Clock* clock,PlayerObject* player,CraftingTool* tool,CraftingStation* station,uint32 expFlag)
{
	CraftingSession* session = new(mSessionPool.ordered_malloc()) CraftingSession(clock,mDatabase,player,tool,station,expFlag);

	return(session);
}

//=============================================================================

void CraftingSessionFactory::destroySession(CraftingSession* session)
{
	if(session)
	{
		session->~CraftingSession();
		mSessionPool.free(session);
	}
}

//=============================================================================


