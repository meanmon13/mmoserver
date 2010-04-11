#include "Utils/typedefs.h"
#include <list>

class CraftingSession;
class Database;
class CraftingTool;
class CraftingSessionFactory;
class PlayerObject;
class CraftingStation;

namespace Anh_Utils
{
	class Clock;
}

typedef std::list<CraftingTool*> CraftTools;

#define gCraftingManager CraftingManager::getSingleton()

class CraftingManager
{
public:
	static CraftingManager*	Init(Database* database);
	static CraftingManager*	getSingleton();
	static void				DestroyInstance();
	bool				Process(uint64 callTime,void* ref);

	//For Work on the Factory
	CraftingSession*	createSession(Anh_Utils::Clock* clock,PlayerObject* player,CraftingTool* tool,CraftingStation* station,uint32 expFlag);
	void				destroySession(CraftingSession* session);

	//For Work for the Timers
	void				addBusyCraftTool(CraftingTool* tool);
	void				removeBusyCraftTool(CraftingTool* tool);

	static CraftingManager* mSingleton;

private:
								CraftingManager(Database* database);
	CraftingSessionFactory*		mCraftingSessionFactory;
	void						CraftingManager::_handleCraftToolTimers(uint64 callTime);

	Database*					mDatabase;

	CraftTools					mBusyCraftTools;
};