#pragma once

#include "AtCrypt.h"
#include "AtEntityStore.h"
#include "AtToken.h"

namespace At
{
	// Entity_GetExtInfo templates

	template <class EntityType, class ExtType>
	ExtType* Entity_GetExtInfo(EntityType& x)
	{
		ExtType* p = nullptr;
		for (Rp<Entity> const& e : x.f_extInfo)
		{
			if (e.Any() && e->GetKind() == ExtType::Kind)
			{
				p = dynamic_cast<ExtType*>(e.Ptr());
				break;
			}
		}
		return p;
	}

	template <class EntityType, class ExtType>
	ExtType const* Entity_GetExtInfo_Const(EntityType const& x)
		{ return const_cast<ExtType const*>(Entity_GetExtInfo<EntityType, ExtType>(const_cast<EntityType&>(x))); }



	// AccessRecord
	// Stored as part of /AppUsers/AppUser.accessRecords, and for general use for objects that need to track access

	ENTITY_DECL_BEGIN(AccessRecord)
	ENTITY_DECL_FIELD(Str,						remoteAddrOnly)				// "Only" meaning just address, no port
	ENTITY_DECL_FIELD(Time,						firstReqTime)
	ENTITY_DECL_FIELD(uint64,					firstRemotePort)
	ENTITY_DECL_FIELD(Time,						recentReqTime)
	ENTITY_DECL_FIELD(uint64,					recentRemotePort)
	ENTITY_DECL_CLOSE();

	class HttpRequest;

	AccessRecord* AccessRecords_FindByAddr(EntVec<AccessRecord>& records, Seq remoteAddrOnly);
	bool AccessRecords_RegisterAccess(EntVec<AccessRecord>& records, HttpRequest& req, bool alwaysUpdate, uint64 onlyUpdateAfterMinutes);


	// LoginSession
	// Stored as part of /AppUsers/AppUser.loginSessions

	ENTITY_DECL_BEGIN(LoginSession)
	ENTITY_DECL_FIELD(Str,						token)
	ENTITY_DECL_FIELD(Time,						createTime)
	ENTITY_DECL_FIELD(Time,						recentReqTime)
	ENTITY_DECL_FIELD(Str,						recentUserAgent)
	ENTITY_DECL_FIELD(Str,						createRemoteAddrOnly)		// "Only" meaning just address, no port
	ENTITY_DECL_FIELD(uint64,					createRemotePort)
	ENTITY_DECL_FIELD(EntVec<AccessRecord>,		accessRecords)
	ENTITY_DECL_FIELD(RpVec<Entity>,			extInfo)
	ENTITY_DECL_CLOSE();

	template <class T> T*       LoginSession_GetExtInfo(LoginSession&           x) { return Entity_GetExtInfo       <LoginSession, T>(x); }
	template <class T> T const* LoginSession_GetExtInfo(LoginSession const&     x) { return Entity_GetExtInfo_Const <LoginSession, T>(x); }
	template <class T> T*       LoginSession_GetExtInfo(Rp<LoginSession> const& x) { if (!x.Any()) return nullptr; return LoginSession_GetExtInfo<T>(x.Ref()); }

	void LoginSession_InitNew(LoginSession& ls, HttpRequest& req);



	// AppUser
	// Stored under /AppUsers

	enum { AppUser_MaxNameLen = 40 };

	ENTITY_DECL_BEGIN(AppUser)
	ENTITY_DECL_FLD_K(Str,						name, KeyCat::Key_Str_Unique_Insensitive)
	ENTITY_DECL_FIELD(Str,						pwHash)
	ENTITY_DECL_FIELD(uint64,					pwVersion)
	ENTITY_DECL_FIELD(Time,						createTime)
	ENTITY_DECL_FIELD(Time,						deactivateTime)			// 0 if remains valid user
	ENTITY_DECL_FIELD(Time,						recentReqTime)			// Not updated for user's every request, but rather every few minutes
	ENTITY_DECL_FIELD(EntVec<LoginSession>,		loginSessions)			// Sorted most stale first, most recently used last
	ENTITY_DECL_FIELD(EntVec<AccessRecord>,		accessRecords)
	ENTITY_DECL_FIELD(RpVec<Entity>,			extInfo)
	ENTITY_DECL_CLOSE();

	Rp<AppUser> GetAppUser(EntityStore& store, Seq name);
	ObjId GetAppUserId(EntityStore& store, Seq name);

	template <class T> T*       AppUser_GetExtInfo(AppUser&           x) { return Entity_GetExtInfo       <AppUser, T>(x); }
	template <class T> T const* AppUser_GetExtInfo(AppUser const&     x) { return Entity_GetExtInfo_Const <AppUser, T>(x); }
	template <class T> T*       AppUser_GetExtInfo(Rp<AppUser> const& x) { if (!x.Any()) return nullptr; return AppUser_GetExtInfo<T>(x.Ref()); }


	// AppUsers
	// Stored under /
	// Used as container for AppUser

	ENTITY_DECL_BEGIN(AppUsers)
	ENTITY_DECL_CLOSE();

	Rp<AppUsers> GetAppUsers(EntityStore& store);
}
