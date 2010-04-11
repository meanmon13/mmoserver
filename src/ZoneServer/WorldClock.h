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
	static void								Init(Database* database);
	static WorldClock*						getSingleton();
	static void								destroySingleton();
	void									ProcessScheduledTasks();
	uint64									GetCurrentGlobalTick();
	bool									handleTick(uint64 callTime,void* ref);

	static WorldClock*						mSingleton;

	Anh_Utils::VariableTimeScheduler*		mVariableTimeScheduler;
	Anh_Utils::Scheduler*					mScheduler;

private:
											WorldClock(Database* database);
											//~WorldClock();

	void									_LoadCurrentGlobalTick();

	uint64									mTick;

	Database*								mDatabase;
};

#define gWorldClock WorldClock::getSingleton()