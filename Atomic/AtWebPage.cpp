#include "AtIncludes.h"
#include "AtWebPage.h"

#include "AtHtmlBuilder.h"
#include "AtPwHash.h"


namespace At
{
	// WebLoginErr

	char const* WebLoginResult::Describe(E n)
	{
		switch (n)
		{
		case WebLoginResult::None:					return "<no login result>";
		case WebLoginResult::Success:				return "Login successful";
		case WebLoginResult::AttemptsTooFrequent:	return "You are trying to log in too frequently; please try again in a little bit";
		case WebLoginResult::InvalidCredentials:	return "Invalid credentials";
		default:									return "<unrecognized login error>";
		}
	}


	// Globals

	LoginThrottle g_webLoginThrottle;


	// WebPage

	ReqResult WebPage::Process(EntityStore& store, HttpRequest& req)
	{
		ReqResult reqResult;
	
		try
		{ 
			if (!WP_ValidateMethod(req))
			{
				m_processError.Init(HttpStatus::MethodNotAllowed);
				reqResult = ReqResult::Continue;
			}
			else
			{
				if (req.IsGetOrHead())
					m_confirmation = CheckCfmCookie(req);

				PageLogin loginType = WP_LoginType();
				switch (loginType)
				{
				case PageLogin::Ignore:
					reqResult = WP_Process(store, req);
					break;

				case PageLogin::IsLoginPage:
					reqResult = ValidateLogin(store, req, loginType);
					if (ReqResult::Continue == reqResult)
					{
						reqResult = Login(store, req);
						if (ReqResult::Continue == reqResult)
							reqResult = WP_Process(store, req);
					}
					break;

				case PageLogin::IsLogoutPage:
					reqResult = ValidateLogin(store, req, loginType);
					if (ReqResult::Continue == reqResult)
					{
						reqResult = Logout(store, req);
						if (ReqResult::Continue == reqResult)
							reqResult = WP_Process(store, req);
					}
					break;

				case PageLogin::Accept:
					reqResult = ValidateLogin(store, req, loginType);
					if (ReqResult::Continue == reqResult)
						reqResult = WP_Process(store, req);
					break; 

				case PageLogin::Require_Relaxed:
				case PageLogin::Require_Strict:
					reqResult = ValidateLogin(store, req, loginType);
					if (ReqResult::Continue == reqResult)
						if (m_login.m_state == Login::State::LoggedIn)
							reqResult = WP_Process(store, req);
						else
						{
							Str loginReturnUrl;
							if (req.IsPost())
								loginReturnUrl = WP_LoginPagePath();
							else
							{
								loginReturnUrl = MakeLoginReturnUrl(req);
								if (m_login.m_state == Login::State::Expired_ThisPage)
								{
									InsensitiveNameValuePairs nvp;
									nvp.Add("loginExpired", "1");
									loginReturnUrl.Add("&cfm=").Add(AddCfmCookieWithContext(req, WP_LoginPageCfmContext(), nvp));
								}
							}

							SetRedirectResponse(HttpStatus::SeeOther, loginReturnUrl);
							reqResult = ReqResult::Done;
						}
					break;

				default:
					reqResult = ReqResult::Invalid;
					EnsureThrowWithNr(!"Unrecognized page login type", (int64) loginType);
				}

				if (ReqResult_IsStatus(reqResult))
				{
					m_processError.Init(ReqResult_GetStatus(reqResult));
					reqResult = ReqResult::Continue;
				}
			}
		}
		catch (HttpRequest::Redirect const& e)
		{
			SetRedirectResponse(e.m_statusCode, e.m_uri);
			reqResult = ReqResult::Done;
		}
		catch (HttpRequest::Error const& e)
		{
			// Allow transaction to conclude, then send the error response in GenResponse()
			m_processError.Init(e);
			reqResult = ReqResult::Continue;
		}

		return reqResult;
	}


	void WebPage::GenResponse(HttpRequest& req)
	{
		if (m_processError.Any())
			WP_GenErrorResponse(req, m_processError->m_statusCode);
		else
			WP_GenResponse(req);
	}


	ReqResult WebPage::Login(EntityStore& store, HttpRequest& req)
	{
		m_loginDest = req.QueryNvp("dest").Trim();
		if (!m_loginDest.Any())
			m_loginDest = WP_DefaultLoginDest();
		else if (!IsInternalRedirDest(m_loginDest))
			return ReqResult::BadRequest;

		if (req.IsGetOrHead())
		{
			if (m_login.m_state == Login::State::LoggedIn)
			{
				// If a logged-in user accesses the login page, redirect to the login destination, UNLESS the user was directed
				// to the login page from another page in a way that suggests the other page has a stricter login session expiry.
				// In this case, let the user log in again so they can access that page.

				if (Confirmation())
				{
					if (req.CfmNvp("loginExpired") == "1")
						m_login.m_state = Login::State::Expired_OtherPage;
				}
				else
				{
					SetRedirectResponse(HttpStatus::SeeOther, m_loginDest);
					return ReqResult::Done;
				}
			}
		}
		else if (req.IsPost())
		{
			Reconstruct(m_login);

			m_login.m_userName =  req.PostNvp("user").Trim();
			Seq password       =  req.PostNvp("pass").Trim();
			m_login.m_remember = (req.PostNvp("remember") == "1");

			m_login.m_loginResult = CheckUserPassword(store, m_login.m_appUser, m_login.m_userName, password, req.RemoteIdAddr());
			if (m_login.m_loginResult == WebLoginResult::Success)
			{
				if (!WP_ValidateAppUser(store, req, m_login.m_expiry))
					m_login.m_loginResult = WebLoginResult::InvalidCredentials;
				else
				{
					// Too many login sessions already?
					sizet maxSessionsPerUser = WP_MaxSessionsPerUser();
					if (m_login.m_appUser->f_loginSessions.Len() >= maxSessionsPerUser)
					{
						// Remove one or more oldest login sessions to make room for a new one
						EnsureThrow(maxSessionsPerUser > 0);
						sizet nrSessionsToErase = (m_login.m_appUser->f_loginSessions.Len() - maxSessionsPerUser) + 1;
						m_login.m_appUser->f_loginSessions.Erase(0, nrSessionsToErase);
					}

					// Insert login token
					LoginSession& loginSession = m_login.m_appUser->f_loginSessions.Add();
					loginSession.f_token.Set(Token::Generate());
					loginSession.f_createTime           = req.RequestTime();
					loginSession.f_createRemoteAddrOnly = req.RemoteAddrOnly();
					loginSession.f_createRemotePort     = req.RemoteAddr().GetPort();
					AccessRecords_RegisterAccess(      loginSession.f_accessRecords, req, true, 0);
					AccessRecords_RegisterAccess(m_login.m_appUser->f_accessRecords, req, true, 0);
					m_login.m_appUser->Update();

					m_login.m_sessionIndex = m_login.m_appUser->f_loginSessions.Len() - 1;
					m_login.m_state = Login::State::LoggedIn;

					SetLoginCookies(req);
					WP_OnLoggedIn(store, req);

					SetRedirectResponse(HttpStatus::SeeOther, m_loginDest);			
					return ReqResult::Done;
				}
			}
		}

		// GET request and not logged in, or login didn't succeed
		return ReqResult::Continue;
	}


	WebLoginResult::E WebPage::CheckUserPassword(EntityStore& store, Rp<AppUser>& appUser, Seq user, Seq password, Seq remoteIdAddr)
	{
		if (g_webLoginThrottle.HaveRecentFailure(user, remoteIdAddr))
			return WebLoginResult::AttemptsTooFrequent;

		appUser = GetAppUser(store, user);
		if (!appUser.Any())
			PwHash::DummyVerify(password);
		else if (PwHash::Verify(password, appUser->f_pwHash))
			return WebLoginResult::Success;

		g_webLoginThrottle.AddLoginFailure(user, remoteIdAddr);
		return WebLoginResult::InvalidCredentials;
	}


	ReqResult WebPage::ValidateLogin(EntityStore& store, HttpRequest& req, PageLogin loginType)
	{
		EnsureThrow(m_login.m_state == Login::State::None);

		m_login.m_userName =  req.CookiesNvp("userName");
		Seq loginToken     =  req.CookiesNvp("loginToken");
		m_login.m_remember = (req.CookiesNvp("rememberLogin") == "1");

		if (m_login.m_userName.Any() && loginToken.n)
		{
			m_login.m_appUser = GetAppUser(store, m_login.m_userName);
			if (m_login.m_appUser.Any())
			{
				LoginSession* loginSession = nullptr;
				for (sizet i=0; i!=m_login.m_appUser->f_loginSessions.Len(); ++i)
				{
					loginSession = &m_login.m_appUser->f_loginSessions[i];
					if (loginSession->f_token == loginToken)
					{
						m_login.m_sessionIndex = i;
						break;
					}
				}
					
				if (m_login.m_sessionIndex != SIZE_MAX &&
					WP_ValidateAppUser(store, req, m_login.m_expiry))
				{
					Time expiryAnchorTime;
					uint expiryMinutes {};
					bool update {};

					if (loginType == PageLogin::Require_Strict)
					{
						Str remoteAddrOnly = Str::From(req.RemoteAddr(), SockAddr::AddrOnly);
						if (Seq(loginSession->f_createRemoteAddrOnly).EqualInsensitive(remoteAddrOnly))
						{
							expiryAnchorTime = loginSession->f_createTime;
							expiryMinutes = m_login.m_expiry.m_strictExpiryMinutes;
						}
					}
					else
					{
						if (!loginSession->f_recentReqTime)
						{
							// This field is a new addition and may be null. In this case, initialize it from access records
							for (AccessRecord const& ar : loginSession->f_accessRecords)
								if (loginSession->f_recentReqTime < ar.f_recentReqTime)
									loginSession->f_recentReqTime = ar.f_recentReqTime;

							update = true;
						}

						expiryAnchorTime = loginSession->f_recentReqTime;
						expiryMinutes = m_login.m_expiry.m_relaxedExpiryMinutes;
					}

					if (req.RequestTime() >= expiryAnchorTime + Time::FromMinutes(expiryMinutes))
					{
						// Not logged in because session is expired for current page.
						// However, we do not remove the session. It may not yet be expired for another page with longer session expiration.
						m_login.m_state = Login::State::Expired_ThisPage;
					}
					else
					{
						// Logged in.
						m_login.m_state = Login::State::LoggedIn;

						// Do we need to update any session-related records?
						uint64 reqTimeUpdateMinutes = WP_AccessUpdateMinutes();
						if (AccessRecords_RegisterAccess (m_login.m_appUser->f_accessRecords, req, update, reqTimeUpdateMinutes)) update = true;
						if (AccessRecords_RegisterAccess (     loginSession->f_accessRecords, req, update, reqTimeUpdateMinutes)) update = true;
						if (update)
						{
							// Place login session at end to maintain sort order (most stale -> most new)
							sizet const lastSessionIndex = m_login.m_appUser->f_loginSessions.Len() - 1;
							if (m_login.m_sessionIndex != lastSessionIndex)
							{
								m_login.m_appUser->f_loginSessions.Add();						// Make room at end
								m_login.m_appUser->f_loginSessions
									.SwapAt(m_login.m_sessionIndex, lastSessionIndex + 1)		// Swap out session to end
									.Erase(m_login.m_sessionIndex, 1);							// Erase where session was before

								m_login.m_sessionIndex = lastSessionIndex;
								loginSession = &m_login.m_appUser->f_loginSessions.Last();
							}

							loginSession->f_recentReqTime = req.RequestTime();
							m_login.m_appUser->f_recentReqTime = req.RequestTime();
						}

						WP_OnLoggedIn(store, req);

						if (update)
						{
							m_login.m_appUser->Update();

							if (!req.IsPost())
							{
								// Do not update login cookies in responses to POST requests. The POST request might be a password change which
								// can result in new login cookies to be sent via ChangePw(). Such cookies would conflict with those generated here.
								// Successful POSTs are expected to be immediately redirected to GET, so postponing cookie update should not usually be an issue.
								SetLoginCookies(req);
							}
						}
					}
				}
			}
		}

		if (m_login.m_state != Login::State::LoggedIn)
		{
			m_login.m_appUser.Clear();
			m_login.m_sessionIndex = SIZE_MAX;
		}

		return ReqResult::Continue;
	}


	ReqResult WebPage::Logout(EntityStore&, HttpRequest& req)
	{
		if (req.IsPost())
		{
			if (m_login.m_state == Login::State::LoggedIn)
			{
				if (req.PostNvp("scope") == "all")
					m_login.m_appUser->f_loginSessions.Clear();
				else
					m_login.m_appUser->f_loginSessions.Erase(m_login.m_sessionIndex, 1);

				m_login.m_appUser->f_recentReqTime = req.RequestTime();
				m_login.m_appUser->Update();

				m_login.m_state = Login::State::LoggedOut;
				m_login.m_appUser.Clear();
				m_login.m_sessionIndex = SIZE_MAX;
			}

			RemoveLoginCookies();
		}
	
		return ReqResult::Continue;
	}


	void WebPage::ChangePw(HttpRequest& req, AppUser& changeAppUser, Seq pw)
	{
		// Ensure we haven't set login cookies in earlier processing of this request
		EnsureThrow(req.IsPost());
		EnsureThrow(WP_LoginType() != PageLogin::IsLoginPage);
		EnsureThrow(WP_LoginType() != PageLogin::IsLogoutPage);

		changeAppUser.f_pwHash = PwHash::Generate(pw);
		changeAppUser.f_pwVersion += 1;
		changeAppUser.f_loginSessions.Clear();

		Str remoteAddrOnly = Str::From(req.RemoteAddr(), SockAddr::AddrOnly);

		if (m_login.m_state != Login::State::LoggedIn || changeAppUser.m_entityId != m_login.m_appUser->m_entityId)
		{
			// Changed another user's password, or not logged in
			AccessRecords_RegisterAccess(changeAppUser.f_accessRecords, req, true, 0);
			changeAppUser.Update();
		}
		else
		{
			// Changed own password. Re-login the user to keep the session
			LoginSession& loginSession = m_login.m_appUser->f_loginSessions.Add();
			loginSession.f_token.Set(Token::Generate());
			loginSession.f_createTime = req.RequestTime();
			AccessRecords_RegisterAccess(      loginSession.f_accessRecords, req, true, 0);
			AccessRecords_RegisterAccess(m_login.m_appUser->f_accessRecords, req, true, 0);
			m_login.m_appUser->Update();

			EnsureThrow(m_login.m_appUser->f_loginSessions.Len() == 1);
			m_login.m_sessionIndex = 0;

			SetLoginCookies(req);
		}
	}


	Str WebPage::MakeLoginReturnUrl(HttpRequest& req) const
	{
		return Str(WP_LoginPagePath()).Add("?dest=").UrlEncode(req.AbsPathAndQS());
	}


	void WebPage::SetLoginCookies(HttpRequest& req)
	{
		EnsureThrow(m_login.m_state == Login::State::LoggedIn);
		EnsureThrow(m_login.m_appUser.Any());

		Seq token { m_login.GetLoginSession().f_token };

		if (m_login.m_remember)
		{
			int expirySeconds { NumCast<int>(m_login.m_expiry.m_relaxedExpiryMinutes * 60) };
			AddSavedCookie(req, "userName", m_login.m_appUser->f_name, Seq(), "/", expirySeconds);
			AddSavedCookie(req, "loginToken", token, Seq(), "/", expirySeconds);
			AddSavedCookie(req, "rememberLogin", "1", Seq(), "/", expirySeconds);
		}
		else
		{
			AddSessionCookie(req, "userName", m_login.m_appUser->f_name, Seq(), "/");
			AddSessionCookie(req, "loginToken", token, Seq(), "/");
			AddSessionCookie(req, "rememberLogin", "0", Seq(), "/");
		}
	}


	void WebPage::RemoveLoginCookies()
	{
		RemoveCookie("userName", Seq(), "/");
		RemoveCookie("loginToken", Seq(), "/");
		RemoveCookie("rememberLogin", Seq(), "/");
	}


	void WebPage::WP_GenErrorResponse(HttpRequest&, uint statusCode)
	{
		m_response.StatusCode = (USHORT) statusCode;

		SetResponseContentType("text/html; charset=UTF-8");
		Seq statusDesc = HttpStatus::Describe(statusCode);	
		HtmlBuilder html;
	
		html.DefaultHead()
				.Title().T(statusDesc).EndTitle()
			.EndHead()
			.Body()
				.P().T("The following error occurred while attempting to access this page: ").EndP()
				.P().T("HTTP status code ").UInt(statusCode).T(": ").T(statusDesc).EndP()
				.P().A().Href("/").T("Return to front page").EndA().EndP()
			.EndBody()
		.EndHtml();
	
		AddResponseBodyChunk(html.Final());
	}
}
