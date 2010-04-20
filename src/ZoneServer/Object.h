 /*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/

#ifndef ANH_ZONESERVER_OBJECT_H
#define ANH_ZONESERVER_OBJECT_H

#include "ObjectController.h"
#include "RadialMenu.h"
#include "UICallback.h"
#include "Object_Enums.h"
#include "LogManager/LogManager.h" // @todo: this needs to go.	  where does it need to go ?
#include "Utils/EventHandler.h"
#include "Utils/typedefs.h"

#include <boost/lexical_cast.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <vector>
#include <map>
#include <set>
#include <list>

#if defined(__GNUC__)
// GCC implements tr1 in the <tr1/*> headers. This does not conform to the TR1
// spec, which requires the header without the tr1/ prefix.
#include <tr1/memory>
#else
#include <memory>
#endif

//=============================================================================

class Object;
class PlayerObject;
class CreatureObject;

typedef std::map<uint32,std::string>	AttributeMap;
typedef std::tr1::shared_ptr<RadialMenu>	RadialMenuPtr;
// typedef std::vector<uint64>				ObjectIDList;
typedef std::list<uint64>				ObjectIDList;
typedef std::set<Object*>				ObjectSet;
typedef std::set<uint64>				ObjectIDSet;
typedef std::set<PlayerObject*>			PlayerObjectSet;
typedef std::set<uint64>			PlayerObjectIDSet;
typedef std::list<uint32>				AttributeOrderList;

//=============================================================================

/*
 - Base class for all gameobjects
 */

class Object : public UICallback, public Anh_Utils::EventHandler
{
	friend class PlayerObjectFactory;
	friend class InventoryFactory;
	friend class NonPersistentItemFactory;


	public:

		Object();
		Object(uint64 id,uint64 parentId,const string model,ObjectType type);

		ObjectLoadState				getLoadState(){ return mLoadState; }
		void						setLoadState(ObjectLoadState state){ mLoadState = state; }

		uint64						getId() const { return mId; }
		void						setId(uint64 id){ mId = id; }

		uint64						getParentId() const { return mParentId; }
		void						setParentId(uint64 parentId){ mParentId = parentId; }
		void						setParentId(uint64 parentId,uint32 contaiment, PlayerObject* target, bool db = false);
		void						setParentId(uint64 parentId,uint32 contaiment, PlayerObjectSet*	knownPlayers, bool db);
		//=============================================================================
		//just sets a new ParentID and sends Containment to TargetObject
		virtual void				setParentIdIncDB(uint64 parentId){mParentId = parentId;
		gLogger->logMsgF("Object::setParentId no table specified id: %I64u",MSG_HIGH,this->getId());}
		
		
		string						getModelString(){ return mModel; }
		ObjectType					getType() const { return mType; }
		uint32						getTypeOptions() const { return mTypeOptions; }

		
		void						setModelString(const string model){ mModel = model; }
		void						setType(ObjectType type){ mType = type; }
		void						setTypeOptions(uint32 options){ mTypeOptions = options; }

		// Object Observers
		PlayerObjectSet*			getKnownPlayers() { return &mKnownPlayers; }
		ObjectSet*					getKnownObjects() { return &mKnownObjects; }
		void						destroyKnownObjects();
		bool						checkKnownPlayer(PlayerObject* player);
		// Not used void						clearKnownObjects(){ mKnownObjects.clear(); mKnownPlayers.clear(); }
		bool						addKnownObjectSafe(Object* object);

		// I wan't to overload this in some classes, instead of adding "useless code" (for the most objects) in the base method.
		virtual void				addKnownObject(Object* object);
		bool						removeKnownObject(Object* object);
		bool						checkKnownObjects(Object* object) const;

		RadialMenuPtr				getRadialMenu(){ return mRadialMenu; }
        virtual void				ResetRadialMenu() {}//	RadialMenu* radial	= NULL;RadialMenuPtr radialPtr(radial);	mRadialMenu = radialPtr;}

		virtual void				handleUIEvent(uint32 action,int32 element,string inputStr = "",UIWindow* window = NULL) {}

		virtual void				prepareCustomRadialMenu(CreatureObject* creatureObject, uint8 itemCount){}
		virtual void				prepareCustomRadialMenuInCell(CreatureObject* creatureObject, uint8 itemCount){}
		virtual	void				handleObjectMenuSelect(uint8 messageType,Object* srcObject){}

		virtual void				updateWorldPosition(){}

		virtual void				sendAttributes(PlayerObject* playerObject);
		virtual string				getBazaarName();
		virtual string				getBazaarTang();

		ObjectController*			getController();

		// common attributes, send to the client
		AttributeMap*				getAttributeMap(){ return &mAttributeMap; }
		template<typename T> T		getAttribute(string key) const;
		template<typename T> T		getAttribute(uint32 keyCrc) const;
		void						setAttribute(string key,std::string value);
		void						setAttributeIncDB(string key,std::string value);
		void						addAttribute(string key,std::string value);
		void						addAttributeIncDB(string key,std::string value);
		bool						hasAttribute(string key) const;
		void						removeAttribute(string key);
		AttributeOrderList*			getAttributeOrder(){ return &mAttributeOrderList; }

		// internal attributes, only used server side
		AttributeMap*				getInternalAttributeMap(){ return &mInternalAttributeMap; }
		template<typename T> T		getInternalAttribute(string key);
		void						setInternalAttribute(string key,std::string value);
		void						addInternalAttribute(string key,std::string value);
		void						setInternalAttributeIncDB(string key,std::string value);
		void						addInternalAttributeIncDB(string key,std::string value);
		bool						hasInternalAttribute(string key);
		void						removeInternalAttribute(string key);

		// subzone
		uint32						getSubZoneId() const { return mSubZoneId; }
		void						setSubZoneId(uint32 id){ mSubZoneId = id; }

		// equip management
		uint32						getEquipSlotMask(){ return mEquipSlots; }
		void						setEquipSlotMask(uint32 slotMask){ mEquipSlots = slotMask; }
		uint32						getEquipRestrictions(){ return mEquipRestrictions; }
		void						setEquipRestrictions(uint32 restrictions){ mEquipRestrictions = restrictions; }

		uint32						getDataTransformCounter(){ return mDataTransformCounter; }
		uint32						incDataTransformCounter(){ return ++mDataTransformCounter; }
		void						setDataTransformCounter(uint32 restrictions){ mDataTransformCounter= restrictions; }

		
		virtual ~Object();

        /*! Retrieve the world position of an object. Important for ranged lookups that need
         *  to include objects inside and outside of buildings.
         *
         * \returns glm::vec3 The world position of an object.
         */
        glm::vec3 getWorldPosition() const;

        /*! Returns the current object's root parent. If the object is the root it returns itself.
         *
         * \returns const Object* Root parent for the current object.
         */
        const Object* getRootParent() const;

        glm::quat   mDirection;
        glm::vec3   mPosition;
		//Anh_Math::Quaternion	mDirection;
		//Anh_Math::Vector3		mPosition;

		inline uint64			getPrivateOwner() { return mPrivateOwner; }
		inline void				setPrivateOwner(uint64 owner) { mPrivateOwner = owner; }
		bool					isOwnedBy(PlayerObject* player);
		const glm::vec3&		getLastUpdatePosition(){ return mLastUpdatePosition; }
		void					setLastUpdatePosition(const glm::vec3& pos ){mLastUpdatePosition = pos; }


		//clientprefab Menu List
		MenuItemList*			getMenuList(){ return mMenuItemList; }
		void					setMenuList(MenuItemList* list){ mMenuItemList = list; }

		bool					movementMessageToggle(){mMovementMessageToggle = !mMovementMessageToggle; return mMovementMessageToggle;}

	protected:

		bool						mMovementMessageToggle;
		AttributeMap				mAttributeMap;
		AttributeOrderList			mAttributeOrderList;
		AttributeMap 				mInternalAttributeMap;
		ObjectSet					mKnownObjects;
		PlayerObjectSet				mKnownPlayers;
		ObjectIDSet					mKnownObjectsIDs;
		ObjectController			mObjectController;
		string						mModel;

		MenuItemList*				mMenuItemList;

		RadialMenuPtr			mRadialMenu;

		ObjectLoadState			mLoadState;
		ObjectType				mType;

		uint64					mId;
		uint64					mParentId;
		uint64					mPrivateOwner; // If object is used as a private object, like in an Instance, we should only update the owner.
		uint32					mEquipRestrictions;
		uint32					mEquipSlots;
		uint32					mInMoveCount;
		uint32					mSubZoneId;
		uint32					mTypeOptions;
		uint32					mDataTransformCounter;
	private:
		glm::vec3		        mLastUpdatePosition;	// Position where SI was updated.

};

//=============================================================================

template<typename T>
T	Object::getAttribute(string key) const
{
	AttributeMap::const_iterator it = mAttributeMap.find(key.getCrc());

	if(it != mAttributeMap.end())
	{
		try
		{
			return(boost::lexical_cast<T>((*it).second));
		}
		catch(boost::bad_lexical_cast &)
		{
			gLogger->logMsgF("Object::getAttribute: cast failed (%s)",MSG_HIGH,key.getAnsi());
		}
	}
	else
		gLogger->logMsgF("Object::getAttribute: could not find %s",MSG_HIGH,key.getAnsi());

	return(T());
}
//=============================================================================

template<typename T>
T	Object::getAttribute(uint32 keyCrc) const
{
	AttributeMap::iterator it = mAttributeMap.find(keyCrc);

	if(it != mAttributeMap.end())
	{
		try
		{
			return(boost::lexical_cast<T>((*it).second));
		}
		catch(boost::bad_lexical_cast &)
		{
			gLogger->logMsgF("Object::getAttribute: cast failed (%s)",MSG_HIGH,keyCrc);
		}
	}
	else
		gLogger->logMsgF("Object::getAttribute: could not find %s",MSG_HIGH,keyCrc);

	return(T());
}


//=============================================================================

template<typename T>
T	Object::getInternalAttribute(string key)
{
	AttributeMap::iterator it = mInternalAttributeMap.find(key.getCrc());

	if(it != mInternalAttributeMap.end())
	{
		try
		{
			return(boost::lexical_cast<T>((*it).second));
		}
		catch(boost::bad_lexical_cast &)
		{
			gLogger->logMsgF("Object::getInternalAttribute: cast failed (%s)",MSG_HIGH,key.getAnsi());
		}
	}
	else
		gLogger->logMsgF("Object::getInternalAttribute: could not find %s",MSG_HIGH,key.getAnsi());

	return(T());
}

//=============================================================================

#endif

