/*
---------------------------------------------------------------------------------------
This source file is part of swgANH (Star Wars Galaxies - A New Hope - Server Emulator)
For more information, see http://www.swganh.org


Copyright (c) 2006 - 2010 The swgANH Team

---------------------------------------------------------------------------------------
*/
#include "TangibleObject.h"
#include "WorldManager.h"
#include "DatabaseManager/Database.h"




//=============================================================================

TangibleObject::TangibleObject()
: ObjectContainer()
, mComplexity(1.0f)
, mLastTimerUpdate(0)
, mTimer(0)
, mDamage(0)
, mMaxCondition(100)
, mTimerInterval(1000)
, mStatic(false)
{
	mType				= ObjType_Tangible;
	mName				= "";
	mNameFile			= "";
	mDetailFile			= "";
	mColorStr			= "";
	mUnknownStr1		= "";
	mUnknownStr2		= "";
	mCustomName			= L"";
	mCustomizationStr	= "";

	//uint64 l = 0;
	for(uint16 i=0;i<256;i++)
		mCustomization[i] = 0;
}

//=============================================================================

TangibleObject::TangibleObject(uint64 id,uint64 parentId,string model,TangibleGroup tanGroup,TangibleType tanType,string name,string nameFile,string detailFile)
			  : ObjectContainer(id,parentId,model,ObjType_Tangible),mName(name),mNameFile(nameFile),mDetailFile(detailFile),mTanGroup(tanGroup),mTanType(tanType)
{
	mColorStr			= "";
	mUnknownStr1		= "";
	mUnknownStr2		= "";
	mCustomName			= L"";
	mCustomizationStr	= "";
}

//=============================================================================

TangibleObject::~TangibleObject()
{
}

//=============================================================================

void TangibleObject::initTimer(int32 count,int32 interval,uint64 startTime)
{
	mTimer				= count;
	mTimerInterval		= interval;
	mLastTimerUpdate	= startTime;
}

//=============================================================================

bool TangibleObject::updateTimer(uint64 callTime)
{
	if(callTime - mLastTimerUpdate >= mTimerInterval)
	{
		mTimer -= (int32)(mTimerInterval / 1000);

		if(mTimer < 0)
			mTimer = 0;

		if(mTimer <= 0)
		{
			mTimer				= 0;
			mLastTimerUpdate	= 0;
			mTimerInterval		= 1000;

			return(false);
		}

		mLastTimerUpdate = callTime;
	}

	return(true);
}

//=============================================================================
//build the customization string based on the rawdata
void TangibleObject::buildTanoCustomization(uint8 len)
{
	uint8* theCustomization = new uint8[512];

	uint16 byteCount = 4; // 2 byte header + 2 byte footer
	uint8 elementCount = 0;

	// get absolute bytecount(1 byte index + value)
	for(uint8 i = 1;i < len;i++)
	{
		if(mCustomization[i] != 0)
		{
			if(mCustomization[i] == 0)
				mCustomization[i] = 511;

			if(mCustomization[i] == 255)
				mCustomization[i] = 767;

			if(mCustomization[i] < 255)
				byteCount += 2;
			else
				byteCount += 3;

			elementCount++;
		}
	}

	if(!elementCount)
	{
		theCustomization = NULL;
		setCustomizationStr(theCustomization);
		return;
	}

	// elements count
	theCustomization[0] = 0x01;
	theCustomization[1] = elementCount;

	uint16 j = 2;

	// fill our string
	for(uint8 i = 1;i < len;i++)
	{
		if(mCustomization[i] != 0)
		{
			theCustomization[j] = i;

			if(mCustomization[i] < 255)
			{
				theCustomization[j+1] = (uint8)(mCustomization[i] & 0xff);
				j += 2;
			}
			else
			{
				theCustomization[j+1] = (uint8)(mCustomization[i] & 0xff);
				theCustomization[j+2] = (uint8)((mCustomization[i] >> 8) & 0xff);
				j += 3;
			}
		}
	}

	// footer
	theCustomization[j] = 0xff;
	theCustomization[j+1] = 0x03;
	theCustomization[j+2] = '\0';

	setCustomizationStr(theCustomization);
	delete [] theCustomization;
}

//=============================================================================
//assign the item a new custom name
//
void TangibleObject::setCustomNameIncDB(const int8* name)
{
	mCustomName = name; 
	int8 sql[1024],restStr[128],*sqlPointer;
	sprintf(sql,"UPDATE items SET customName='");
		sqlPointer = sql + strlen(sql);
		sqlPointer += gWorldManager->getDatabase()->Escape_String(sqlPointer,mCustomName.getAnsi(),mCustomName.getLength());
		sprintf(restStr,"' WHERE id=%"PRIu64" ",this->getId());
	
	strcat(sql,restStr);
	gWorldManager->getDatabase()->ExecuteSqlAsync(0,0,sql);
}

//=============================================================================
//assign the item a new parentid
//

void TangibleObject::setParentIdIncDB(uint64 parentId)
{ 
	mParentId = parentId; 
	gWorldManager->getDatabase()->ExecuteSqlAsync(0,0,"UPDATE items SET parent_id=%"PRIu64" WHERE id=%"PRIu64"",mParentId,this->getId());
}


void TangibleObject::prepareCustomRadialMenuInCell(CreatureObject* creatureObject, uint8 itemCount)
{
	RadialMenu* radial	= new RadialMenu();
	uint8 i = 1;
	uint8 u = 1;

	// any object with callbacks needs to handle those (received with menuselect messages) !
	if(this->getObjects()->size())
		radial->addItem(i++,0,radId_itemOpen,radAction_Default,"");

	radial->addItem(i++,0,radId_examine,radAction_Default,"");

	radial->addItem(i++,0,radId_itemPickup,radAction_Default,"");
	
	u = i;
	radial->addItem(i++,0,radId_itemMove,radAction_Default, "");	
	radial->addItem(i++,u,radId_itemMoveForward,radAction_Default, "");//radAction_ObjCallback
	radial->addItem(i++,u,radId_ItemMoveBack,radAction_Default, "");
	radial->addItem(i++,u,radId_itemMoveUp,radAction_Default, "");
	radial->addItem(i++,u,radId_itemMoveDown,radAction_Default, "");
	
	u = i;
	radial->addItem(i++,0,radId_itemRotate,radAction_Default, "");
	radial->addItem(i++,u,radId_itemRotateRight,radAction_Default, "");
	radial->addItem(i++,u,radId_itemRotateLeft,radAction_Default, "");

  
	RadialMenuPtr radialPtr(radial);
	mRadialMenu = radialPtr;


}