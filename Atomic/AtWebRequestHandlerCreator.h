#pragma once

#include "AtWebRequestHandler.h"


namespace At
{
	struct ReqTxExclusive { enum E { No, IfPost, Always }; };

	struct WebRequestHandlerCreator : public RefCountable
	{
		virtual ~WebRequestHandlerCreator() {}
	
		// The maximum entity body size that should be accepted.
		// Handler creators for pages that need to process larger bodies should override this method.
		static sizet const MaxBodySize_Default = 16000;
		virtual sizet MaxBodySize() const { return MaxBodySize_Default; }

		// Return true if POST requests should run in exclusive transaction.
		// Use this if the transaction is expected to be large, and highly likely to be retried otherwise.
		virtual ReqTxExclusive::E ReqTxExclusivity() const { return ReqTxExclusive::No; }

		// Creates the WebRequestHandler.
		virtual Rp<WebRequestHandler> Create() const = 0;
	};


	template <sizet MBS, ReqTxExclusive::E RTX>
	struct GenericWebRequestHandlerCreatorBase : WebRequestHandlerCreator
	{
		sizet             MaxBodySize      () const override final { return MBS; }
		ReqTxExclusive::E ReqTxExclusivity () const override final { return RTX; }
	};

	template <class WRH, sizet MBS=WebRequestHandlerCreator::MaxBodySize_Default, ReqTxExclusive::E RTX=ReqTxExclusive::No>
	struct GenericWebRequestHandlerCreator : GenericWebRequestHandlerCreatorBase<MBS, RTX>
		{ Rp<WebRequestHandler> Create() const override final { return new WRH(); } };

	template <class WRH, sizet MBS=WebRequestHandlerCreator::MaxBodySize_Default, ReqTxExclusive::E RTX=ReqTxExclusive::No>
	struct GenericWebRequestHandlerCreator_UInt : GenericWebRequestHandlerCreatorBase<MBS, RTX>
	{
		GenericWebRequestHandlerCreator_UInt(uint64 a) : m_a(a) {}
		Rp<WebRequestHandler> Create() const override final { return new WRH(m_a); }
		uint64 m_a;
	};

	template <class WRH, sizet MBS=WebRequestHandlerCreator::MaxBodySize_Default, ReqTxExclusive::E RTX=ReqTxExclusive::No>
	struct GenericWebRequestHandlerCreator_UInt2 : GenericWebRequestHandlerCreatorBase<MBS, RTX>
	{
		GenericWebRequestHandlerCreator_UInt2(uint64 a, uint64 b) : m_a(a), m_b(b) {}
		Rp<WebRequestHandler> Create() const override final { return new WRH(m_a, m_b); }
		uint64 m_a, m_b;
	};

	template <class WRH, sizet MBS=WebRequestHandlerCreator::MaxBodySize_Default, ReqTxExclusive::E RTX=ReqTxExclusive::No>
	struct GenericWebRequestHandlerCreator_UInt3 : GenericWebRequestHandlerCreatorBase<MBS, RTX>
	{
		GenericWebRequestHandlerCreator_UInt3(uint64 a, uint64 b, uint64 c) : m_a(a), m_b(b), m_c(c) {}
		Rp<WebRequestHandler> Create() const override final { return new WRH(m_a, m_b, m_c); }
		uint64 m_a, m_b, m_c;
	};

	template <class WRH, sizet MBS=WebRequestHandlerCreator::MaxBodySize_Default, ReqTxExclusive::E RTX=ReqTxExclusive::No>
	struct GenericWebRequestHandlerCreator_Str : GenericWebRequestHandlerCreatorBase<MBS, RTX>
	{
		GenericWebRequestHandlerCreator_Str(Seq a) : m_a(a) {}
		Rp<WebRequestHandler> Create() const override final { return new WRH(m_a); }
		Str m_a;
	};

	template <class WRH, sizet MBS=WebRequestHandlerCreator::MaxBodySize_Default, ReqTxExclusive::E RTX=ReqTxExclusive::No>
	struct GenericWebRequestHandlerCreator_Str2 : GenericWebRequestHandlerCreatorBase<MBS, RTX>
	{
		GenericWebRequestHandlerCreator_Str2(Seq a, Seq b) : m_a(a), m_b(b) {}
		Rp<WebRequestHandler> Create() const override final { return new WRH(m_a, m_b); }
		Str m_a, m_b;
	};

	template <class WRH, sizet MBS=WebRequestHandlerCreator::MaxBodySize_Default, ReqTxExclusive::E RTX=ReqTxExclusive::No>
	struct GenericWebRequestHandlerCreator_Str3 : GenericWebRequestHandlerCreatorBase<MBS, RTX>
	{
		GenericWebRequestHandlerCreator_Str3(Seq a, Seq b, Seq c) : m_a(a), m_b(b), m_c(c) {}
		Rp<WebRequestHandler> Create() const override final { return new WRH(m_a, m_b, m_c); }
		Str m_a, m_b, m_c;
	};


	class WebRequestHandler_Redirect : public WebRequestHandler
	{
	public:
		class Creator : public WebRequestHandlerCreator
		{
		public:
			Creator(uint statusCode, Seq uri) : m_statusCode(statusCode), m_uri(uri) {}
			Rp<WebRequestHandler> Create() const override { return new WebRequestHandler_Redirect(m_statusCode, m_uri); }
		private:
			uint m_statusCode;
			Str m_uri;
		};

		WebRequestHandler_Redirect(uint statusCode, Seq uri) : m_statusCode(statusCode), m_uri(uri) {}

		ReqResult Process(EntityStore&, HttpRequest&) override final { SetRedirectResponse(m_statusCode, m_uri); return ReqResult::Done; }
		void GenResponse(HttpRequest&) override final { throw NotImplemented(); }

	private:
		uint m_statusCode;
		Str m_uri;
	};
}
