#include "WorldClock.h"

#include "Utils/Scheduler.h"
#include "Utils/VariableTimeScheduler.h"

#include "DatabaseManager/Database.h"
#include "DatabaseManager/DataBinding.h"
#include "DatabaseManager/DatabaseResult.h"

#include "LogManager/LogManager.h"

WorldClock* WorldClock::mSingleton = NULL;

void WorldClock::Init(Database* database)
{
	if(!mSingleton)
		mSingleton = new WorldClock(database);
}

WorldClock* WorldClock::getSingleton()
{
	return mSingleton;
}

void WorldClock::destroySingleton()
{
	if(mSingleton)
		delete mSingleton;
}

void WorldClock::ProcessScheduledTasks()
{
	mScheduler->process();
	mVariableTimeScheduler->process();
}

WorldClock::WorldClock(Database* database)
{
	mDatabase = database;

	mVariableTimeScheduler = new Anh_Utils::VariableTimeScheduler(100, 100);
	mScheduler = new Anh_Utils::Scheduler();

	_LoadCurrentGlobalTick();
}

//======================================================================================================================
//get the current tick
//

uint64 WorldClock::GetCurrentGlobalTick()
{
	return mTick;
}

//======================================================================================================================
//still synch issues to adress with other servers
//

void WorldClock::_LoadCurrentGlobalTick()
{
	uint64 Tick;
	DatabaseResult* temp = mDatabase->ExecuteSynchSql("SELECT Global_Tick_Count FROM galaxy WHERE galaxy_id = '2'");

	DataBinding*	tickbinding = mDatabase->CreateDataBinding(1);
	tickbinding->addField(DFT_uint64,0,8,0);

	temp->GetNextRow(tickbinding,&Tick);
	mDatabase->DestroyDataBinding(tickbinding);
	mDatabase->DestroyResult(temp);

	char strtemp[100];
	sprintf(strtemp, "Current Global Tick Count = %"PRIu64"\n",Tick);
	gLogger->logMsg(strtemp, FOREGROUND_GREEN);
	mTick = Tick;
	mScheduler->addTask(fastdelegate::MakeDelegate(this,&WorldClock::handleTick),7,1000,NULL);
}

//======================================================================================================================
//
//

bool	WorldClock::handleTick(uint64 callTime,void* ref)
{
	mTick += 1000;
	return true;
}