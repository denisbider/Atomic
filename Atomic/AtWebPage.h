#pragma once

#include "AtLoginThrottle.h"
#include "AtToken.h"
#include "AtWebEntities.h"
#include "AtWebRequestHandlerCreator.h"

namespace At
{
	// - Ignore will not test for a login, will not validate it, and will not make any login info available to the application.
	// - IsLoginPage and IsLogoutPage should be used only by pages dedicated to login and logout functionality.
	// - Accept does not require a login, but will validate it and make the info available to the application.
	// - Require_Relaxed and Require_Strict both require a login, and will not permit the page to be processed without it.
	//   If the user is not logged in or their login session expired, the user will be redirected to the login page.
	// - Require_Relaxed allows the user to avoid session expiry with regular session activity. It allows a session
	//   to be continued through changes in the remote address: for example, if the user receives a new IP from their ISP.
	// - Require_Strict uses a shorter expiration and does NOT accept a session that's been extended through activity.
	//   It does not accept access from a remote address other than that used to perform the login.
	enum class PageLogin { Ignore, IsLoginPage, IsLogoutPage, Accept, Require_Relaxed, Require_Strict };

	struct WebLoginResult
	{
		enum E { None, Success, AttemptsTooFrequent, InvalidCredentials };
		static char const* Describe(E n);
	};

	struct WebSessionExpiry
	{
		uint m_relaxedExpiryMinutes {};
		uint m_strictExpiryMinutes  {};
	};


	extern LoginThrottle g_webLoginThrottle;


	class WebPage : public WebRequestHandler
	{
	public:
		ReqResult Process     (EntityStore& store, HttpRequest& req) override final;
		void      GenResponse (HttpRequest& req)                     override final;

		virtual bool      WP_ValidateMethod  (HttpRequest& req) const                        { return req.IsGetOrHead(); }
		virtual PageLogin WP_LoginType       () const                                        = 0;
		virtual bool      WP_ValidateAppUser (EntityStore&, HttpRequest&, WebSessionExpiry&) { throw ZLitErr("WP_ValidateAppUser not implemented"); }
		virtual void      WP_OnLoggedIn      (EntityStore&, HttpRequest&)                    { }
		virtual ReqResult WP_Process         (EntityStore&, HttpRequest&)                    { return ReqResult::Continue; }
		virtual void      WP_GenResponse     (HttpRequest&)                                  { throw ZLitErr("WP_GenResponse not implemented"); }
		virtual void      WP_GenErrorResponse(HttpRequest& req, uint statusCode);

		virtual Seq		  WP_LoginPagePath       () const { throw ZLitErr("WP_LoginPagePath not implemented"); }
		virtual Seq       WP_LoginPageCfmContext () const { throw ZLitErr("WP_LoginPageCfmContext not implemented"); }
		virtual Seq		  WP_DefaultLoginDest    () const { return "/"; }
		virtual sizet     WP_MaxSessionsPerUser  () const { return   3; }
		virtual uint      WP_AccessUpdateMinutes () const { return   5; }
	
	public:
		Opt<HttpRequest::Error> m_processError;

		struct Login
		{
			Str               m_userName;
			bool              m_remember             {};

			enum class State { None, LoggedIn, Expired_ThisPage, Expired_OtherPage, LoggedOut };
			State             m_state                { State::None };
			WebLoginResult::E m_loginResult          { WebLoginResult::None };
			WebSessionExpiry  m_expiry;

			Rp<AppUser>       m_appUser;
			sizet             m_sessionIndex         { SIZE_MAX };

			LoginSession& GetLoginSession() { return m_appUser->f_loginSessions[m_sessionIndex]; }
		};

		Login m_login;
		Str   m_loginDest;

		bool IsInternalRedirDest(Seq dest) const { return dest.StartsWithExact("/") && !dest.StartsWithExact("//"); }
		Str MakeLoginReturnUrl(HttpRequest& req) const;

		// Updates AppUser
		void ChangePw(HttpRequest& req, AppUser& changeAppUser, Seq pw);

		void SetLoginCookies(HttpRequest& req);
		void RemoveLoginCookies();

		static WebLoginResult::E CheckUserPassword(EntityStore& store, Rp<AppUser>& appUser, Seq user, Seq password, Seq remoteIdAddr);

	private:
		bool m_confirmation {};

		ReqResult Login         (EntityStore& store, HttpRequest& req);
		ReqResult ValidateLogin (EntityStore& store, HttpRequest& req, PageLogin loginType);
		ReqResult Logout        (EntityStore& store, HttpRequest& req);

	protected:
		bool Confirmation() const { return m_confirmation; }
	};



	class WebPage_Error : public WebPage
	{
	public:
		using Creator = GenericWebRequestHandlerCreator_UInt<WebPage_Error>;

		WebPage_Error(uint64 statusCode) : m_statusCode((uint) statusCode) {}

		PageLogin WP_LoginType () const                     override final { return PageLogin::Ignore; }
		ReqResult WP_Process   (EntityStore&, HttpRequest&) override final { m_processError.Init(m_statusCode); return ReqResult::Continue; }

	private:
		uint m_statusCode;
	};


	// To redirect, use WebRequestHandler_Redirect.

}
