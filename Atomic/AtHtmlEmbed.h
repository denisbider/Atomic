#pragma once

#include "AtHtmlBuilder.h"
#include "AtMap.h"
#include "AtOpt.h"
#include "AtPinStore.h"

namespace At
{
	namespace Html
	{
		// EmbedCx

		enum class AbsContentUrlBeh		// Behavior for embedded content URLs that specify an absolute URL (not relative, not fragment ID)
		{
			Remove,		// Remove absolute content URLs
			Preload,	// Replace absolute content URLs with deterministic relative URLs, populate EmbedCx::m_preloadUrls so the remote URLs can be preloaded
			Preserve,	// Preserve absolute content URLs as they are, allowing for direct embedding of remote content
		};

		struct PreloadUrl
		{
			Seq m_origUrl;
			Seq m_destUrl;

			Seq Key() const { return m_origUrl; }
		};

		struct ElemEmbedCx : NoCopy
		{
			bool m_hasNonFragmentUrl {};
		};

		struct EmbedCx : NoCopy
		{
			EmbedCx(PinStore& store) : m_store(store) {}

			// Input parameters
			PinStore&        m_store;
			Seq              m_idPrefix;
			AbsContentUrlBeh m_absContentUrlBeh {};

			// Initialized during embedding
			sizet            m_nrAbsContentUrls {};
			Map<PreloadUrl>  m_preloadUrls;
			ElemEmbedCx      m_elemCx;					// Reconstruct this between elements

			void OnNewElem() { Reconstruct(m_elemCx); }

			virtual Seq GetUrlForCid(Seq cid) = 0;			// Should return a URL (relative or absolute) that attempts to serve the resource identified by Content-ID
		};


		// AttrInfo

		struct EmbedAction { enum E { Invalid, Omit, Allow }; };

		typedef Seq (*EmbedSanitizer)(Seq value, EmbedCx& cx);

		struct AttrInfo
		{
			char const*    m_tag;					// nullptr indicates last entry in list
			EmbedAction::E m_embedAction;
			EmbedSanitizer m_embedSanitizer;
		};



		// ElemInfo

		struct ElemType   { enum E { Invalid, Void, NonVoid }; };

		typedef void (*EmbedFinalizer)(HtmlBuilder& html, EmbedCx& cx);

		struct ElemInfo
		{
			char const*     m_tag;					// nullptr indicates last entry in list
			ElemType::E     m_type;
			EmbedAction::E  m_embedAction;
			AttrInfo const* m_attrs;				// Element-specific attributes. Global attributes need to be searched in addition
			EmbedFinalizer  m_finalizer;

			AttrInfo const* FindAttrInfo_ByTagExact(Seq tagLower) const;
		};

		extern ElemInfo const c_elemInfo[];

		// Returns nullptr if tag not found in c_elemInfo
		ElemInfo const* FindElemInfo_ByTagExact(Seq tagLower);
	}
}
