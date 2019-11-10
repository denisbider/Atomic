#include "AtIncludes.h"
#include "AtWebEntities.h"

#include "AtHttpRequest.h"
#include "AtInitOnFirstUse.h"


namespace At
{

	// AccessRecord
	
	ENTITY_DEF_BEGIN(AccessRecord)
	ENTITY_DEF_FIELD(AccessRecord, remoteAddrOnly)
	ENTITY_DEF_FIELD(AccessRecord, firstReqTime)
	ENTITY_DEF_FIELD(AccessRecord, firstRemotePort)
	ENTITY_DEF_FIELD(AccessRecord, recentReqTime)
	ENTITY_DEF_FIELD(AccessRecord, recentRemotePort)
	ENTITY_DEF_CLOSE(AccessRecord);

	
	AccessRecord* AccessRecords_FindByAddr(EntVec<AccessRecord>& records, Seq remoteAddrOnly)
	{
		for (AccessRecord& ar : records)
			if (Seq(ar.f_remoteAddrOnly).EqualInsensitive(remoteAddrOnly))
				return &ar;

		return nullptr;
	}


	bool AccessRecords_RegisterAccess(EntVec<AccessRecord>& records, HttpRequest& req, uint64 onlyUpdateAfterMinutes)
	{
		Time onlyUpdateAfterPeriod = Time::FromMinutes(onlyUpdateAfterMinutes);

		AccessRecord* ar = AccessRecords_FindByAddr(records, req.RemoteAddrOnly());
		if (ar)
		{
			if (req.RequestTime() - ar->f_recentReqTime < onlyUpdateAfterPeriod)
				return false;

			ar->f_recentReqTime    = req.RequestTime();
			ar->f_recentRemotePort = req.RemoteAddr().GetPort();
			return true;
		}

		// No match, insert new record
		ar = &records.Add();
		ar->f_remoteAddrOnly   = req.RemoteAddrOnly();
		ar->f_firstReqTime     = req.RequestTime();
		ar->f_firstRemotePort  = req.RemoteAddr().GetPort();
		ar->f_recentReqTime    = req.RequestTime();
		ar->f_recentRemotePort = req.RemoteAddr().GetPort();
		return true;
	}



	// LoginSession

	ENTITY_DEF_BEGIN(LoginSession)
	ENTITY_DEF_FIELD(LoginSession, token)
	ENTITY_DEF_FIELD(LoginSession, createTime)
	ENTITY_DEF_FIELD(LoginSession, createRemoteAddrOnly)
	ENTITY_DEF_FIELD(LoginSession, createRemotePort)
	ENTITY_DEF_FIELD(LoginSession, accessRecords)
	ENTITY_DEF_FIELD(LoginSession, extInfo)
	ENTITY_DEF_CLOSE(LoginSession);



	// AppUser

	ENTITY_DEF_BEGIN(AppUser)
	ENTITY_DEF_FIELD(AppUser, name)
	ENTITY_DEF_FIELD(AppUser, pwHash)
	ENTITY_DEF_FIELD(AppUser, pwVersion)
	ENTITY_DEF_FIELD(AppUser, createTime)
	ENTITY_DEF_FIELD(AppUser, deactivateTime)
	ENTITY_DEF_FIELD(AppUser, recentReqTime)
	ENTITY_DEF_FIELD(AppUser, loginSessions)
	ENTITY_DEF_FIELD(AppUser, accessRecords)
	ENTITY_DEF_FIELD(AppUser, extInfo)
	ENTITY_DEF_CLOSE(AppUser);

	
	Rp<AppUser> GetAppUser(EntityStore& store, Seq name)
	{
		Rp<AppUser> appUser;
		Rp<AppUsers> appUsers { GetAppUsers(store) };
		if (appUsers.Any())
			appUser = appUsers->FindChild<AppUser>(name);
		return appUser;
	}


	ObjId GetAppUserId(EntityStore& store, Seq name)
	{
		ObjId appUserId;
		Rp<AppUsers> appUsers { GetAppUsers(store) };
		if (appUsers.Any())
			appUserId = appUsers->FindChildId<AppUser>(name);
		return appUserId;
	}



	// AppUsers

	ENTITY_DEF_BEGIN(AppUsers)
	ENTITY_DEF_CLOSE(AppUsers);


	LONG volatile g_appUsersFlag {};
	Rp<AppUsers>  g_appUsers;

	Rp<AppUsers> GetAppUsers(EntityStore& store)
	{
		InitOnFirstUse(&g_appUsersFlag, [&store]
			{
				g_appUsers.Set(store.GetFirstChildOfKind<AppUsers>(ObjId::Root));
				if (!g_appUsers.Any())
				{
					g_appUsers = new AppUsers(store, ObjId::Root);
					g_appUsers->Insert_ParentExists();
				}
			} );

		return g_appUsers;
	}

}
