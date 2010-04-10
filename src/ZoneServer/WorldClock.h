#include "Utils/typedefs.h"

class Database;

namespace Anh_Utils
{
    class Clock;
    class Scheduler;
    class VariableTimeScheduler;
}

class WorldClock
{
public:
	static void									Init(Database* database);
	static WorldClock*								getSingleton();
	static void									destroySingleton();

	//void									AddTask();
	//void									AddVariableTimeTask();
	void									ProcessScheduledTasks();

	uint64									GetCurrentGlobalTick();

	static WorldClock*						mSingleton;
	
	bool									handleTick(uint64 callTime,void* ref);

private:
											WorldClock(Database* database);
											//~WorldClock();

	void									_LoadCurrentGlobalTick();

	Anh_Utils::VariableTimeScheduler*		mVariableTimeScheduler;
	Anh_Utils::Scheduler*					mScheduler;

	uint64									mTick;

	Database*								mDatabase;
};

#define gWorldClock WorldClock::getSingleton()