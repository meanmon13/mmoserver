/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/

#ifndef ANH_ZONESERVER_TANGIBLE_DATAPAD_H
#define ANH_ZONESERVER_TANGIBLE_DATAPAD_H

#include "TangibleObject.h"
#include "ObjectFactoryCallback.h"


//=============================================================================

class IntangibleObject;
class ManufacturingSchematic;
class MissionObject;
class PlayerObject;
class WaypointObject;

typedef std::list<WaypointObject*>			WaypointList;
typedef std::list<ManufacturingSchematic*>	ManufacturingSchematicList;
typedef std::list<MissionObject*>				MissionList;
typedef std::list<IntangibleObject*>			DataList;

//=============================================================================

class Datapad : public TangibleObject
{
	friend class DatapadFactory;

	public:

		Datapad();
		~Datapad();

		// inherited callback
		virtual void	handleObjectReady(Object* object,DispatchClient* client);

		// player reference
		void			setOwner(PlayerObject* player){ mOwner = player; }

		// waypoints
		WaypointList*	getWaypoints(){ return &mWaypoints; }
		WaypointObject*	getWaypointById(uint64 id);
		WaypointObject*	getWaypointByName(string name);
		bool			addWaypoint(WaypointObject* waypoint);
		bool			removeWaypoint(uint64 id);
		bool			removeWaypoint(WaypointObject* waypoint);
		void			setObjectLoadCounter(uint32 count){ mObjectLoadCounter = count; }
		void			requestNewWaypoint(string name, const glm::vec3& coords, uint16 planetId, uint8 wpType);

		//missions
		MissionList*   getMissions() { return &mMissions; }
		MissionObject* getMissionById(uint64 id);
		bool		   addMission(MissionObject* mission);
		bool		   removeMission(uint64 id);
		bool		   removeMission(MissionObject* mission);
		bool		   hasMission(){return( mMissions.size()>0);}
		
		MissionList::iterator removeMission(MissionList::iterator it);

		//data -- aka mounts,pets,vehicles, etc
		DataList*		   getData() { return &mData; }
		IntangibleObject*  getDataById(uint64 id);
		bool		       addData(IntangibleObject* Data);
		bool		       removeData(uint64 id);
		bool		       removeData(IntangibleObject* Data);
		DataList::iterator removeData(DataList::iterator it);

		// ManufacturingSchematics
		ManufacturingSchematicList*		getManufacturingSchematics(){ return &mManufacturingSchematics; }
		ManufacturingSchematic*			getManufacturingSchematicById(uint64 id);

		
		bool							addManufacturingSchematic(ManufacturingSchematic* ms);
		bool							removeManufacturingSchematic(uint64 id);
		bool							removeManufacturingSchematic(ManufacturingSchematic* ms);


		// capacity
		uint8			getCapacity(){ return mCapacity; }
		uint8			getMissionCapacity() { return mMissionCapacity; }
		
		uint32			mWaypointUpdateCounter;
		uint32			mManSUpdateCounter;
		uint32			mSchematicUpdateCounter;
		uint32			mMissionUpdateCounter;

	private:

		uint8						mCapacity;
		uint8						mWayPointCapacity;
		uint8						mMissionCapacity;
		WaypointList				mWaypoints;
		ManufacturingSchematicList	mManufacturingSchematics;
		MissionList					mMissions;
		DataList					mData;
		PlayerObject*				mOwner;
		uint32						mObjectLoadCounter;
};

//=============================================================================

#endif

