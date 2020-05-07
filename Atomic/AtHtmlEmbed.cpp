#include "AtIncludes.h"
#include "AtHtmlEmbed.h"

#include "AtHtmlGrammar.h"
#include "AtMimeGrammar.h"
#include "AtSha512.h"
#include "AtUriGrammar.h"
#include "AtToken.h"


namespace At
{
	namespace Html
	{
		// AttrInfo

		namespace
		{
			struct UnsafeChars { enum E { End, Encode }; };

			void SanitizeValue(Seq value, sizet maxNrBytes, CharCriterion safeCharCrit, UnsafeChars::E unsafeChars, Vec<Seq>& v, EmbedCx& cx)
			{
				// ASCII alphanumeric characters and select ASCII punctuation are passed through unchanged
				// Each sequence of HTML whitespace is replaced with single space
				// Characters <= 0xFF              are encoded as $hh
				// Characters >  0xFF && <= 0xFFFF are encoded as %hhhh
				// Characters >  0xFFFF            are encoded as !hhhhhh

				sizet nrBytes {};
				while (value.n)
				{
					EnsureThrow(nrBytes < maxNrBytes);

					// Whitespace?
					Seq ws { value.ReadToFirstByteNotOf(Html::c_htmlWsChars) };
					if (ws.n)
					{
						v.Add(" ");
						if (++nrBytes == maxNrBytes)
							break;
					}

					// Sequence of safe bytes?
					bool atUnsafeByte {};
					sizet nrSafeBytes {};
					while (value.n > nrSafeBytes)
					{
						byte b { value.p[nrSafeBytes] };
						if (safeCharCrit(b))
						{
							++nrSafeBytes;
							if (nrBytes + nrSafeBytes == maxNrBytes)
								break;
						}
						else
						{
							atUnsafeByte = true;
							break;
						}
					}

					if (nrSafeBytes)
					{
						v.Add(value.ReadBytes(nrSafeBytes));
						nrBytes += nrSafeBytes;
						if (nrBytes == maxNrBytes)
							break;
					}

					// Unsafe byte?
					if (atUnsafeByte)
					{
						if (unsafeChars == UnsafeChars::End)
							break;

						uint c { value.ReadUtf8Char() };
						EnsureThrow(c <= 0xFFFFFF);		// Input should be valid UTF-8, and we should not be at end of value

						byte  prefix;
						sizet n;

						     if (c <=   0xFF) { prefix = '$'; n = 1; }
						else if (c <= 0xFFFF) { prefix = '%'; n = 2; }
						else                  { prefix = '!'; n = 3; }

						Enc::Write write  = cx.m_store.IncWrite(1 + (2*n));
						byte*      pStart = write.Ptr();
						byte*      pWrite = pStart;

						*pWrite++ = prefix;
						do
						{
							--n;
							byte b { (byte) ((c >> (8*n)) & 0xFF) };
							*pWrite++ = (byte) NumAlphabet::Upper[(b >> 4) & 0x0F];
							*pWrite++ = (byte) NumAlphabet::Upper[(b     ) & 0x0F];
						}
						while (n);

						Seq encoded { pStart, write.AddUpTo(pWrite) };
						v.Add(encoded);
						nrBytes += encoded.n;
						if (nrBytes >= maxNrBytes)
							break;
					}
				}
			}

			Seq SanitizeValue(Seq value, sizet maxNrBytes, CharCriterion safeCharCrit, UnsafeChars::E unsafeChars, EmbedCx& cx)
			{
				Vec<Seq> v;
				SanitizeValue(value, maxNrBytes, safeCharCrit, unsafeChars, v, cx);

				if (!v.Any())     return Seq();
				if (v.Len() == 1) return v.First();
				return cx.m_store.AddStrFromParts(v);
			}

			enum { MaxKeywordBytes=100, MaxKeywordListBytes=1000, MaxMimeTypeBytes=100, MaxMimeTypeListBytes=1000,
			       MaxColorValueBytes=20, MaxFileNameBytes=150, MaxCoordListBytes=1000, MaxNumValueBytes=100, MaxDateTimeBytes=50,
			       MaxTextValueBytes=1000, MaxContentTypeValueBytes=1000, MaxIdBytes=100, MaxIdsPerAttrValue=30 };

			bool IsSafeKeywordChar      (uint c) { return Ascii::IsAlphaNum (c) || ZChr("-_",    c) != nullptr; }
			bool IsSafeKeywordListChar  (uint c) { return Ascii::IsAlphaNum (c) || ZChr("-_,",   c) != nullptr; }
			bool IsSafeMimeTypeChar     (uint c) { return Ascii::IsAlphaNum (c) || ZChr("-_/,",  c) != nullptr; }
			bool IsSafeMimeTypeListChar (uint c) { return Ascii::IsAlphaNum (c) || ZChr("-_/,",  c) != nullptr; }
			bool IsSafeColorValueChar   (uint c) { return Ascii::IsAlphaNum (c) || ZChr("#",     c) != nullptr; }
			bool IsSafeFileNameChar     (uint c) { return Ascii::IsAlphaNum (c) || ZChr("-+_#.", c) != nullptr; }
			bool IsSafeCoordListChar    (uint c) { return Ascii::IsDecDigit (c) || ZChr(",%",    c) != nullptr; }
			bool IsSafeNumValueChar     (uint c) { return Ascii::IsDecDigit (c) || ZChr("+-.",   c) != nullptr; }
			bool IsSafeProportionChar   (uint c) { return Ascii::IsDecDigit (c) || ZChr(".%*",   c) != nullptr; }
			bool IsSafeNumOrKeywordChar (uint c) { return Ascii::IsAlphaNum (c) || ZChr("+-_.",  c) != nullptr; }
			bool IsSafeDateTimeChar     (uint c) { return Ascii::IsAlphaNum (c) || ZChr("+-:.",  c) != nullptr; }
			bool IsSafeIdChar           (uint c) { return Ascii::IsAlphaNum (c) || ZChr("-_",    c) != nullptr; }

			Seq KeywordSanitizer      (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxKeywordBytes,      IsSafeKeywordChar,      UnsafeChars::End,    cx); }
			Seq KeywordListSanitizer  (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxKeywordListBytes,  IsSafeKeywordListChar,  UnsafeChars::End,    cx); }
			Seq MimeTypeSanitizer     (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxMimeTypeBytes,     IsSafeMimeTypeChar,     UnsafeChars::End,    cx); }
			Seq MimeTypeListSanitizer (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxMimeTypeListBytes, IsSafeMimeTypeListChar, UnsafeChars::End,    cx); }
			Seq ColorValueSanitizer   (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxColorValueBytes,   IsSafeColorValueChar,   UnsafeChars::End,    cx); }
			Seq FileNameSanitizer     (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxFileNameBytes,     IsSafeFileNameChar,     UnsafeChars::Encode, cx); }
			Seq CoordListSanitizer    (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxCoordListBytes,    IsSafeCoordListChar,    UnsafeChars::End,    cx); }
			Seq NumOrKeywordSanitizer (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxKeywordBytes,      IsSafeNumOrKeywordChar, UnsafeChars::End,    cx); }
			Seq NumValueSanitizer     (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxNumValueBytes,     IsSafeNumValueChar,     UnsafeChars::End,    cx); }
			Seq ProportionSanitizer   (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxNumValueBytes,     IsSafeProportionChar,   UnsafeChars::End,    cx); }
			Seq DateTimeSanitizer     (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxDateTimeBytes,     IsSafeDateTimeChar,     UnsafeChars::End,    cx); }
			Seq NoPrefixIdSanitizer   (Seq value, EmbedCx& cx) { return SanitizeValue(value, MaxIdBytes,           IsSafeIdChar,           UnsafeChars::Encode, cx); }

			Seq TextValueSanitizer (Seq value, EmbedCx&) { return value.ReadUtf8_MaxBytes(MaxTextValueBytes); }
			Seq CharValueSanitizer (Seq value, EmbedCx&) { return value.ReadUtf8_MaxChars(1); }


			void ForEachHtmlWsSeparatedToken(Seq value, sizet maxNrTokens, std::function<void (Seq token)> action)
			{
				sizet nrTokens {};
				while (value.n)
				{
					value.DropToFirstByteNotOf(c_htmlWsChars);
					Seq token { value.ReadToFirstByteOf(c_htmlWsChars) };
					if (token.n)
					{
						action(token);
						if (++nrTokens >= maxNrTokens)
							break;
					}
				}
			}

			Seq IdSanitizer(Seq value, EmbedCx& cx)
			{
				// We use same treatment for fields where we expect a single ID as for fields where we expect multiple.
				// If the string has whitespace, but is going to be treated as a single ID, inserting multiple prefixes
				// will not harm things as long as we do it consistently. On the other hand, it is far better to add
				// prefixes within the string in case it might be interpreted by a browser as multiple IDs.

				if (!cx.m_idPrefix.n)
					return NoPrefixIdSanitizer(value, cx);

				Vec<Seq> v;
				ForEachHtmlWsSeparatedToken(value, MaxIdsPerAttrValue,
					[&] (Seq token)
					{
						if (v.Any())
							v.Add(" ");

						v.Add(cx.m_idPrefix);
						SanitizeValue(token, MaxIdBytes, IsSafeIdChar, UnsafeChars::Encode, v, cx);
					} );

				return cx.m_store.AddStrFromParts(v);
			}


			Seq HashIdSanitizer(Seq value, EmbedCx& cx)
			{
				if (!cx.m_idPrefix.n)
					return NoPrefixIdSanitizer(value, cx);

				Vec<Seq> v;
				ForEachHtmlWsSeparatedToken(value, MaxIdsPerAttrValue,
					[&] (Seq token)
					{
						if (v.Any())
							v.Add(" ");

						token.StripPrefixExact("#");
						v.Add("#");
						v.Add(cx.m_idPrefix);
						SanitizeValue(token, MaxIdBytes, IsSafeIdChar, UnsafeChars::Encode, v, cx);
					} );

				return cx.m_store.AddStrFromParts(v);
			}


			Seq InputTypeSanitizer(Seq value, EmbedCx& cx)
			{
				// Embedded HTML should not ask for password input
				Seq sanitized { KeywordSanitizer(value, cx).Trim() };
				if (sanitized.EqualInsensitive("password"))
					return "hidden";

				return sanitized;
			}


			Seq const c_urlRemoved_relUrlNotPermitted  { "javascript:void('URL removed: Relative URL not permitted')"                            };
			Seq const c_urlRemoved_absUrlNoAuthority   { "javascript:void('URL removed: An absolute URL MUST include an authority')"             };
			Seq const c_urlRemoved_absUrlNotPermitted  { "javascript:void('URL removed: Absolute URL not permitted')"                            };
			Seq const c_urlRemoved_schemeNotPermitted  { "javascript:void('URL removed: The URL scheme is not permitted')"                       };
			Seq const c_urlRemoved_parseError          { "javascript:void('URL removed: The URL could not be parsed')"                           };
			Seq const c_urlRemoved_noValidUrls         { "javascript:void('URL removed: Multi-URL attribute value did not contain a valid URL')" };

			Seq SanitizeAbsUrl_Remove   (Seq,     EmbedCx&) { return c_urlRemoved_absUrlNotPermitted; }
			Seq SanitizeAbsUrl_Preserve (Seq url, EmbedCx&) { return url; }

			Seq SanitizeAbsUrl_Preload(Seq url, EmbedCx& cx)
			{
				// Already known URL?
				Map<PreloadUrl>::It it { cx.m_preloadUrls.Find(url) };
				if (it.Any())
					return it->m_destUrl;

				// Generate dest URL deterministically
				byte digest[SHA512_DigestSize];
				SHA512_Simple(url.p, NumCast<uint32>(url.n), digest);
				static_assert(SHA512_DigestSize >= Token::RawLen, "Raw token len unexpectedly large");
				Seq destUrl { Token::Tokenize(Seq(digest, Token::RawLen), cx.m_store.GetEnc(Token::Len)) };

				// Add orig + dest URL pair
				it = cx.m_preloadUrls.Add();
				it->m_origUrl = url;
				it->m_destUrl = destUrl;
				return destUrl;
			}


			Seq SanitizeUrl(ParseNode const& uriNode, EmbedCx& cx, Seq (*sanitizeAbsUrl)(Seq url, EmbedCx&))
			{
				if (uriNode.FirstChild().IsType(Uri::id_fragment))
				{
					// Self-reference fragment
					return HashIdSanitizer(uriNode.SrcText(), cx);
				}

				Seq scheme;
				ParseNode const* schemeNode { uriNode.FrontFind(Uri::id_scheme) };
				if (schemeNode)
					scheme = schemeNode->SrcText();

				if (!scheme.n)
					return c_urlRemoved_relUrlNotPermitted;

				if (scheme.EqualInsensitive("data"))
				{
					cx.m_elemCx.m_hasNonFragmentUrl = true;
					return uriNode.SrcText();
				}

				if (scheme.EqualInsensitive("cid"))
				{
					cx.m_elemCx.m_hasNonFragmentUrl = true;
					Seq cid = schemeNode->Remaining();
					cid.StripPrefixExact(":");
					return cx.GetUrlForCid(cid);
				}

				if (scheme.EqualInsensitive("http") || scheme.EqualInsensitive("https"))
				{
					ParseNode const* authorityNode { uriNode.DeepFind(Uri::id_authority) };
					if (!authorityNode)
						return c_urlRemoved_absUrlNoAuthority;

					// Have authority, and scheme is http or https: true absolute URL
					++(cx.m_nrAbsContentUrls);
					cx.m_elemCx.m_hasNonFragmentUrl = true;
					return sanitizeAbsUrl(uriNode.SrcText(), cx);
				}

				return c_urlRemoved_schemeNotPermitted;
			}


			Seq SanitizeUrl(Seq value, EmbedCx& cx, Seq (*sanitizeAbsUrl)(Seq url, EmbedCx&))
			{
				value.DropToFirstByteNotOf(c_htmlWsChars);
				if (value.StartsWithInsensitive("data:"))
					return value;

				ParseTree pt { value };
				if (pt.Parse(Uri::C_URI_reference))
					return SanitizeUrl(pt.Root(), cx, sanitizeAbsUrl);

				return c_urlRemoved_parseError;
			}


			Seq LinkUrlSanitizer(Seq value, EmbedCx& cx) { return SanitizeUrl(value, cx, SanitizeAbsUrl_Preserve  ); }

			Seq ContentUrlSanitizer(Seq value, EmbedCx& cx)
			{
				switch (cx.m_absContentUrlBeh)
				{
				case AbsContentUrlBeh::Remove:   return SanitizeUrl(value, cx, SanitizeAbsUrl_Remove);
				case AbsContentUrlBeh::Preload:  return SanitizeUrl(value, cx, SanitizeAbsUrl_Preload);
				case AbsContentUrlBeh::Preserve: return SanitizeUrl(value, cx, SanitizeAbsUrl_Preserve);
				default: EnsureThrow(!"Unrecognized AbsContentUrlBeh value"); return Seq();
				}
			}


			Seq MultiUrlSanitizer(Seq value, EmbedCx& cx)
			{
				value.DropToFirstByteNotOf(c_htmlWsChars);

				Vec<Seq> urls;
				ParseTree pt { value };
				if (pt.Parse(Uri::C_URI_references))
					for (ParseNode const& c : pt.Root())
						if (c.IsType(Uri::id_URI_reference))
						{
							Seq url { SanitizeUrl(c, cx, SanitizeAbsUrl_Preserve) };
							if (url.n)
							{
								if (urls.Any())
									urls.Add(" ");
								urls.Add(url);
							}
						}

				if (!urls.Any())
					return c_urlRemoved_noValidUrls;

				if (urls.Len() == 1)
					return urls.First();

				return cx.m_store.AddStrFromParts(urls);
			}


			AttrInfo const c_globalAttrInfo[]
				{
					{ "accesskey",          EmbedAction::Omit,  nullptr                            }, // Not embeddable, can hijack access keys used by rest of page
					{ "class",              EmbedAction::Omit,  nullptr                            }, // Not embeddable for now, need CSS parsing & transform
					{ "contextmenu",        EmbedAction::Omit,  nullptr                            }, // Not embeddable, could replace container page's context menu
					{ "draggable",          EmbedAction::Omit,  nullptr                            }, // Not embeddable, could cause unexpected interactions with parent page
					{ "dropzone",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, could cause unexpected interactions with parent page
					{ "style",              EmbedAction::Omit,  nullptr                            }, // Currently not embeddable, requires CSS parsing
					{ "tabindex",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, can mess with tab index of container page

					{ "contenteditable",    EmbedAction::Allow, KeywordSanitizer                   },
					{ "dir",                EmbedAction::Allow, KeywordSanitizer                   },
					{ "hidden",             EmbedAction::Allow, KeywordSanitizer                   },
					{ "id",                 EmbedAction::Allow, IdSanitizer                        },
					{ "lang",               EmbedAction::Allow, KeywordSanitizer                   },
					{ "spellcheck",         EmbedAction::Allow, KeywordSanitizer                   },
					{ "title",              EmbedAction::Allow, TextValueSanitizer                 },
					{ "translate",          EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_a_attrInfo[]
				{
					{ "referrer",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not control referrer policy
					{ "rel",                EmbedAction::Omit,  nullptr                            }, // Not embeddable, can affect the whole page
					{ "target",             EmbedAction::Omit,  nullptr                            }, // Not embeddable; set by finalizer if a link is present

					{ "download",           EmbedAction::Allow, FileNameSanitizer                  },
					{ "href",               EmbedAction::Allow, LinkUrlSanitizer                   },
					{ "hreflang",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "media",              EmbedAction::Allow, TextValueSanitizer                 },
					{ "name",               EmbedAction::Allow, IdSanitizer                        },
					{ "ping",               EmbedAction::Allow, MultiUrlSanitizer                  },
					{ "type",               EmbedAction::Allow, MimeTypeSanitizer                  },
					{ nullptr                                                                      },
				};

			void a_Finalizer(HtmlBuilder& html, EmbedCx& cx)
			{
				// If opener is permitted, new tab can redirect our tab to a phishing page. This will be evident by URL, and there may be other ways to do it,
				// but we prefer to not make it trivial. We prefer to not leak our URL in referrer info, either.
				if (cx.m_elemCx.m_hasNonFragmentUrl)
					html.Target("_blank").Rel("noopener noreferrer");
			}


			AttrInfo const c_area_attrInfo[]
				{
					{ "referrer",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not control referrer policy
					{ "rel",                EmbedAction::Omit,  nullptr                            }, // Not embeddable, can affect the whole page
					{ "tabindex",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, can mess with tab index of container page
					{ "target",             EmbedAction::Omit,  nullptr                            }, // Not embeddable; set by finalizer if a link is present

					{ "alt",                EmbedAction::Allow, TextValueSanitizer                 },
					{ "coords",             EmbedAction::Allow, CoordListSanitizer                 },
					{ "download",           EmbedAction::Allow, FileNameSanitizer                  },
					{ "href",               EmbedAction::Allow, LinkUrlSanitizer                   },
					{ "hreflang",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "media",              EmbedAction::Allow, TextValueSanitizer                 },
					{ "name",               EmbedAction::Allow, IdSanitizer                        },
					{ "nohref",             EmbedAction::Allow, KeywordSanitizer                   },
					{ "shape",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "type",               EmbedAction::Allow, MimeTypeSanitizer                  },
					{ nullptr                                                                      },
				};

			void area_Finalizer(HtmlBuilder& html, EmbedCx& cx)
			{
				// If opener is permitted, new tab can redirect our tab to a phishing page. This will be evident by URL, and there may be other ways to do it,
				// but we prefer to not make it trivial. We prefer to not leak our URL in referrer info, either.
				if (cx.m_elemCx.m_hasNonFragmentUrl)
					html.Target("_blank").Rel("noopener noreferrer");
			}

			AttrInfo const c_audio_attrInfo[]
				{
					{ "autoplay",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, autoplay in HTML email would not be welcomed by user

					{ "controls",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "loop",               EmbedAction::Allow, KeywordSanitizer                   },
					{ "muted",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "preload",            EmbedAction::Allow, KeywordSanitizer                   },
					{ "src",                EmbedAction::Allow, ContentUrlSanitizer                },
					{ "volume",             EmbedAction::Allow, NumValueSanitizer                  },
					{ nullptr                                                                      },
				};

			AttrInfo const c_blockquote_attrInfo[]
				{
					{ "cite",               EmbedAction::Allow, LinkUrlSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_button_attrInfo[]
				{
					{ "autofocus",          EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not be able to steal focus
					{ "formtarget",         EmbedAction::Omit,  nullptr                            }, // Not embeddable; "_top" target is already set on <form> element

					{ "disabled",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "form",               EmbedAction::Allow, IdSanitizer                        },
					{ "formaction",         EmbedAction::Allow, LinkUrlSanitizer                   },
					{ "formenctype",        EmbedAction::Allow, MimeTypeSanitizer                  },
					{ "formmethod",         EmbedAction::Allow, KeywordSanitizer                   },
					{ "formnovalidate",     EmbedAction::Allow, KeywordSanitizer                   },
					{ "name",               EmbedAction::Allow, NoPrefixIdSanitizer                },
					{ "type",               EmbedAction::Allow, KeywordSanitizer                   },
					{ "value",              EmbedAction::Allow, TextValueSanitizer                 },
					{ nullptr                                                                      },
				};

			AttrInfo const c_caption_attrInfo[]
				{
					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_col_colgroup_attrInfo[]
				{
					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "bgcolor",            EmbedAction::Allow, ColorValueSanitizer                },
					{ "char",               EmbedAction::Allow, CharValueSanitizer                 },
					{ "charoff",            EmbedAction::Allow, NumValueSanitizer                  },
					{ "span",               EmbedAction::Allow, NumValueSanitizer                  },
					{ "valign",             EmbedAction::Allow, KeywordSanitizer                   },
					{ "width",              EmbedAction::Allow, ProportionSanitizer                },
					{ nullptr                                                                      },
				};

			AttrInfo const c_data_attrInfo[]
				{
					{ "value",              EmbedAction::Allow, TextValueSanitizer                 },
					{ nullptr                                                                      },
				};

			AttrInfo const c_dd_attrInfo[]
				{
					{ "nowrap",             EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_div_attrInfo[]
				{
					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_del_ins_attrInfo[]
				{
					{ "cite",               EmbedAction::Allow, LinkUrlSanitizer                   },
					{ "datetime",           EmbedAction::Allow, DateTimeSanitizer                  },
					{ nullptr                                                                      },
				};

			AttrInfo const c_details_attrInfo[]
				{
					{ "open",               EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_fieldset_attrInfo[]
				{
					{ "disabled",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "form",               EmbedAction::Allow, IdSanitizer                        },
					{ "name",               EmbedAction::Allow, NoPrefixIdSanitizer                },
					{ nullptr                                                                      },
				};

			AttrInfo const c_form_attrInfo[]
				{
					{ "autocomplete",       EmbedAction::Omit,  nullptr                            }, // Not embeddable; set to "off" in finalizer
					{ "target",             EmbedAction::Omit,  nullptr                            }, // Not embeddable; set by finalizer if a link is present

					{ "accept",             EmbedAction::Allow, MimeTypeListSanitizer              },
					{ "accept-charset",     EmbedAction::Allow, KeywordListSanitizer               },
					{ "action",             EmbedAction::Allow, LinkUrlSanitizer                   },
					{ "autocapitalize",     EmbedAction::Allow, KeywordSanitizer                   },
					{ "enctype",            EmbedAction::Allow, MimeTypeSanitizer                  },
					{ "method",             EmbedAction::Allow, KeywordSanitizer                   },
					{ "name",               EmbedAction::Allow, IdSanitizer                        },
					{ "novalidate",         EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			void form_Finalizer(HtmlBuilder& html, EmbedCx& cx)
			{
				html.AutoComplete("off");
				if (cx.m_elemCx.m_hasNonFragmentUrl)
					html.Target("_top");	// As of August 25, 2016, browsers appear to support rel="noopener" only for <a> and <area>. Therefore, make sure to replace whole current page.
			}

			AttrInfo const c_hr_attrInfo[]
				{
					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "color",              EmbedAction::Allow, ColorValueSanitizer                },
					{ "noshade",            EmbedAction::Allow, KeywordSanitizer                   },
					{ "size",               EmbedAction::Allow, NumValueSanitizer                  },
					{ "width",              EmbedAction::Allow, ProportionSanitizer                },
					{ nullptr                                                                      },
				};

			AttrInfo const c_img_attrInfo[]
				{
					{ "crossorigin",        EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not be using cross-origin resource sharing
					{ "referrer",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not control referrer policy

					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "alt",                EmbedAction::Allow, TextValueSanitizer                 },
					{ "border",             EmbedAction::Allow, NumValueSanitizer                  },
					{ "height",             EmbedAction::Allow, ProportionSanitizer                },
					{ "hspace",             EmbedAction::Allow, ProportionSanitizer                },
					{ "ismap",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "name",               EmbedAction::Allow, IdSanitizer                        },
					{ "src",                EmbedAction::Allow, ContentUrlSanitizer                },
					{ "usemap",             EmbedAction::Allow, HashIdSanitizer                    },
					{ "vspace",             EmbedAction::Allow, ProportionSanitizer                },
					{ "width",              EmbedAction::Allow, ProportionSanitizer                },
					{ nullptr                                                                      },
				};

			AttrInfo const c_input_attrInfo[]
				{
					{ "autocomplete",       EmbedAction::Omit,  nullptr                            }, // Not embeddable; set to "off" in finalizer
					{ "autofocus",          EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not be able to steal focus
					{ "autosave",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not have state
					{ "formtarget",         EmbedAction::Omit,  nullptr                            }, // Not embeddable; "_top" target is already set on <form> element
					{ "pattern",            EmbedAction::Omit,  nullptr                            }, // Not embeddable; requires allowing a full regexp with complex interpretation

					{ "accept",             EmbedAction::Allow, MimeTypeListSanitizer              },
					{ "autocapitalize",     EmbedAction::Allow, KeywordSanitizer                   },
					{ "autocorrect",        EmbedAction::Allow, KeywordSanitizer                   },
					{ "checked",            EmbedAction::Allow, KeywordSanitizer                   },
					{ "disabled",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "form",               EmbedAction::Allow, IdSanitizer                        },
					{ "formaction",         EmbedAction::Allow, LinkUrlSanitizer                   },
					{ "formenctype",        EmbedAction::Allow, MimeTypeSanitizer                  },
					{ "formmethod",         EmbedAction::Allow, KeywordSanitizer                   },
					{ "formnovalidate",     EmbedAction::Allow, KeywordSanitizer                   },
					{ "height",             EmbedAction::Allow, ProportionSanitizer                },
					{ "incremental",        EmbedAction::Allow, KeywordSanitizer                   },
					{ "inputmode",          EmbedAction::Allow, KeywordSanitizer                   },
					{ "list",               EmbedAction::Allow, IdSanitizer                        },
					{ "max",                EmbedAction::Allow, DateTimeSanitizer                  },
					{ "maxlength",          EmbedAction::Allow, NumValueSanitizer                  },
					{ "min",                EmbedAction::Allow, DateTimeSanitizer                  },
					{ "minlength",          EmbedAction::Allow, NumValueSanitizer                  },
					{ "multiple",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "name",               EmbedAction::Allow, NoPrefixIdSanitizer                },
					{ "placeholder",        EmbedAction::Allow, TextValueSanitizer                 },
					{ "readonly",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "required",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "results",            EmbedAction::Allow, NumValueSanitizer                  },
					{ "selectiondirection", EmbedAction::Allow, KeywordSanitizer                   },
					{ "size",               EmbedAction::Allow, NumValueSanitizer                  },
					{ "spellcheck",         EmbedAction::Allow, KeywordSanitizer                   },
					{ "src",                EmbedAction::Allow, ContentUrlSanitizer                },
					{ "step",               EmbedAction::Allow, NumOrKeywordSanitizer              },
					{ "type",               EmbedAction::Allow, InputTypeSanitizer                 },
					{ "usemap",             EmbedAction::Allow, HashIdSanitizer                    },
					{ "value",              EmbedAction::Allow, TextValueSanitizer                 },
					{ "width",              EmbedAction::Allow, ProportionSanitizer                },
					{ nullptr                                                                      },
				};

			void input_Finalizer(HtmlBuilder& html, EmbedCx&)
			{
				html.AutoComplete("off");
			}

			AttrInfo const c_label_attrInfo[]
				{
					{ "for",                EmbedAction::Allow, IdSanitizer                        },
					{ "form",               EmbedAction::Allow, IdSanitizer                        },
					{ nullptr                                                                      },
				};

			AttrInfo const c_li_attrInfo[]
				{
					{ "type",               EmbedAction::Allow, KeywordSanitizer                   },
					{ "value",              EmbedAction::Allow, NumValueSanitizer                  },
					{ nullptr                                                                      },
				};

			AttrInfo const c_map_attrInfo[]
				{
					{ "name",               EmbedAction::Allow, IdSanitizer                        },
					{ nullptr                                                                      },
				};

			AttrInfo const c_meter_attrInfo[]
				{
					{ "form",               EmbedAction::Allow, IdSanitizer                        },
					{ "high",               EmbedAction::Allow, NumValueSanitizer                  },
					{ "low",                EmbedAction::Allow, NumValueSanitizer                  },
					{ "max",                EmbedAction::Allow, NumValueSanitizer                  },
					{ "min",                EmbedAction::Allow, NumValueSanitizer                  },
					{ "optimum",            EmbedAction::Allow, NumValueSanitizer                  },
					{ "value",              EmbedAction::Allow, NumValueSanitizer                  },
					{ nullptr                                                                      },
				};

			AttrInfo const c_ol_attrInfo[]
				{
					{ "compact",            EmbedAction::Allow, KeywordSanitizer                   },
					{ "reversed",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "start",              EmbedAction::Allow, NumValueSanitizer                  },
					{ "type",               EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_optgroup_attrInfo[]
				{
					{ "disabled",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "label",              EmbedAction::Allow, TextValueSanitizer                 },
					{ nullptr                                                                      },
				};

			AttrInfo const c_option_attrInfo[]
				{
					{ "disabled",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "label",              EmbedAction::Allow, TextValueSanitizer                 },
					{ "selected",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "value",              EmbedAction::Allow, TextValueSanitizer                 },
					{ nullptr                                                                      },
				};

			AttrInfo const c_output_attrInfo[]
				{
					{ "for",                EmbedAction::Allow, IdSanitizer                        },
					{ "form",               EmbedAction::Allow, IdSanitizer                        },
					{ "name",               EmbedAction::Allow, NoPrefixIdSanitizer                },
					{ nullptr                                                                      },
				};

			AttrInfo const c_p_attrInfo[]
				{
					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_pre_attrInfo[]
				{
					{ "cols",               EmbedAction::Allow, NumValueSanitizer                  },
					{ "width",              EmbedAction::Allow, NumValueSanitizer                  },
					{ "wrap",               EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_progress_attrInfo[]
				{
					{ "max",                EmbedAction::Allow, NumValueSanitizer                  },
					{ "value",              EmbedAction::Allow, NumValueSanitizer                  },
					{ nullptr                                                                      },
				};

			AttrInfo const c_q_attrInfo[]
				{
					{ "cite",               EmbedAction::Allow, LinkUrlSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_select_attrInfo[]
				{
					{ "autofocus",          EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not be able to steal focus

					{ "disabled",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "form",               EmbedAction::Allow, IdSanitizer                        },
					{ "multiple",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "name",               EmbedAction::Allow, NoPrefixIdSanitizer                },
					{ "required",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "size",               EmbedAction::Allow, NumValueSanitizer                  },
					{ nullptr                                                                      },
				};

			AttrInfo const c_source_attrInfo[]
				{
					{ "src",                EmbedAction::Allow, ContentUrlSanitizer                },
					{ "type",               EmbedAction::Allow, MimeTypeSanitizer                  },
					{ "media",              EmbedAction::Allow, TextValueSanitizer                 },
					{ nullptr                                                                      },
				};

			AttrInfo const c_table_attrInfo[]
				{
					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "bgcolor",            EmbedAction::Allow, ColorValueSanitizer                },
					{ "border",             EmbedAction::Allow, NumValueSanitizer                  },
					{ "cellpadding",        EmbedAction::Allow, ProportionSanitizer                },
					{ "cellspacing",        EmbedAction::Allow, ProportionSanitizer                },
					{ "frame",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "rules",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "summary",            EmbedAction::Allow, TextValueSanitizer                 },
					{ "width",              EmbedAction::Allow, ProportionSanitizer                },
					{ nullptr                                                                      },
				};

			AttrInfo const c_td_th_attrInfo[]
				{
					{ "axis",               EmbedAction::Omit,  nullptr                            }, // Not embeddable: not supported by browsers, contains IDs; unclear how to treat safely
					{ "scope",              EmbedAction::Omit,  nullptr                            }, // Not embeddable: not supported by browsers, unclear how to treat safely

					{ "abbr",               EmbedAction::Allow, TextValueSanitizer                 },
					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "bgcolor",            EmbedAction::Allow, ColorValueSanitizer                },
					{ "char",               EmbedAction::Allow, CharValueSanitizer                 },
					{ "charoff",            EmbedAction::Allow, NumValueSanitizer                  },
					{ "colspan",            EmbedAction::Allow, NumValueSanitizer                  },
					{ "headers",            EmbedAction::Allow, IdSanitizer                        },
					{ "rowspan",            EmbedAction::Allow, NumValueSanitizer                  },
					{ "valign",             EmbedAction::Allow, KeywordSanitizer                   },
					{ "width",              EmbedAction::Allow, ProportionSanitizer                },
					{ nullptr                                                                      },
				};

			AttrInfo const c_textarea_attrInfo[]
				{
					{ "autocomplete",       EmbedAction::Omit,  nullptr                            }, // Not embeddable; set to "off" in finalizer
					{ "autofocus",          EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not be able to steal focus

					{ "autocapitalize",     EmbedAction::Allow, KeywordSanitizer                   },
					{ "cols",               EmbedAction::Allow, NumValueSanitizer                  },
					{ "disabled",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "form",               EmbedAction::Allow, IdSanitizer                        },
					{ "maxlength",          EmbedAction::Allow, NumValueSanitizer                  },
					{ "minlength",          EmbedAction::Allow, NumValueSanitizer                  },
					{ "name",               EmbedAction::Allow, NoPrefixIdSanitizer                },
					{ "placeholder",        EmbedAction::Allow, TextValueSanitizer                 },
					{ "readonly",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "required",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "rows",               EmbedAction::Allow, NumValueSanitizer                  },
					{ "spellcheck",         EmbedAction::Allow, KeywordSanitizer                   },
					{ "wrap",               EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_tr_tbody_tfoot_thead_attrInfo[]
				{
					{ "align",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "bgcolor",            EmbedAction::Allow, ColorValueSanitizer                },
					{ "char",               EmbedAction::Allow, CharValueSanitizer                 },
					{ "charoff",            EmbedAction::Allow, NumValueSanitizer                  },
					{ "valign",             EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_time_attrInfo[]
				{
					{ "datetime",           EmbedAction::Allow, DateTimeSanitizer                  },
					{ nullptr                                                                      },
				};

			AttrInfo const c_track_attrInfo[]
				{
					{ "default",            EmbedAction::Allow, KeywordSanitizer                   },
					{ "kind",               EmbedAction::Allow, KeywordSanitizer                   },
					{ "label",              EmbedAction::Allow, TextValueSanitizer                 },
					{ "src",                EmbedAction::Allow, ContentUrlSanitizer                },
					{ "srclang",            EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_ul_attrInfo[]
				{
					{ "compact",            EmbedAction::Allow, KeywordSanitizer                   },
					{ "type",               EmbedAction::Allow, KeywordSanitizer                   },
					{ nullptr                                                                      },
				};

			AttrInfo const c_video_attrInfo[]
				{
					{ "autoplay",           EmbedAction::Omit,  nullptr                            }, // Not embeddable, autoplay in HTML email would not be welcomed by user
					{ "crossorigin",        EmbedAction::Omit,  nullptr                            }, // Not embeddable, contained HTML should not be using cross-origin resource sharing

					{ "controls",           EmbedAction::Allow, KeywordSanitizer                   },
					{ "height",             EmbedAction::Allow, NumValueSanitizer                  },
					{ "loop",               EmbedAction::Allow, KeywordSanitizer                   },
					{ "muted",              EmbedAction::Allow, KeywordSanitizer                   },
					{ "poster",             EmbedAction::Allow, ContentUrlSanitizer                },
					{ "preload",            EmbedAction::Allow, KeywordSanitizer                   },
					{ "src",                EmbedAction::Allow, ContentUrlSanitizer                },
					{ "width",              EmbedAction::Allow, NumValueSanitizer                  },
					{ nullptr                                                                      },
				};

		} // anon


		// ElemInfo

		ElemInfo const c_elemInfo[]
			{
				// Meta

				{ "base",       ElemType::Void,    EmbedAction::Omit                                           }, // Not embeddable, only one per HTML page
				{ "head",       ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, only one per HTML page
				{ "html",       ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, only one per HTML page
				{ "link",       ElemType::Void,    EmbedAction::Omit                                           }, // Not embeddable, affects whole HTML page
				{ "meta",       ElemType::Void,    EmbedAction::Omit                                           }, // Not embeddable, affects whole HTML page
				{ "style",      ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable automatically, requires CSS parsing
				{ "title",      ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, affects whole HTML page

				// Sections

				{ "body",       ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, only one per HTML page

				{ "address",    ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "article",    ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "aside",      ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "footer",     ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "header",     ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "h1",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "h2",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "h3",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "h4",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "h5",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "h6",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "hgroup",     ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "nav",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "section",    ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },

				// Flow content

				{ "main",       ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, affects whole HTML page

				{ "blockquote", ElemType::NonVoid, EmbedAction::Allow, c_blockquote_attrInfo                   },
				{ "center",     ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "details",    ElemType::NonVoid, EmbedAction::Allow, c_details_attrInfo                      },
				{ "dd",         ElemType::NonVoid, EmbedAction::Allow, c_dd_attrInfo                           },
				{ "div",        ElemType::NonVoid, EmbedAction::Allow, c_div_attrInfo                          },
				{ "dl",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "dt",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "figcaption", ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "figure",     ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "hr",         ElemType::Void,    EmbedAction::Allow, c_hr_attrInfo                           },
				{ "li",         ElemType::NonVoid, EmbedAction::Allow, c_li_attrInfo                           },
				{ "ol",         ElemType::NonVoid, EmbedAction::Allow, c_ol_attrInfo                           },
				{ "p",          ElemType::NonVoid, EmbedAction::Allow, c_p_attrInfo                            },
				{ "pre",        ElemType::NonVoid, EmbedAction::Allow, c_pre_attrInfo                          },
				{ "ul",         ElemType::NonVoid, EmbedAction::Allow, c_ul_attrInfo                           },

				// Phrasing content

				{ "a",          ElemType::NonVoid, EmbedAction::Allow, c_a_attrInfo,           a_Finalizer     },
				{ "abbr",       ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "b",          ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "bdi",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "bdo",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "big",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "br",         ElemType::Void,    EmbedAction::Allow, nullptr                                 },
				{ "cite",       ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "code",       ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "data",       ElemType::NonVoid, EmbedAction::Allow, c_data_attrInfo                         },
				{ "dfn",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "del",        ElemType::NonVoid, EmbedAction::Allow, c_del_ins_attrInfo                      },
				{ "em",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "i",          ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "ins",        ElemType::NonVoid, EmbedAction::Allow, c_del_ins_attrInfo                      },
				{ "kbd",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "mark",       ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "nobr",       ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "q",          ElemType::NonVoid, EmbedAction::Allow, c_q_attrInfo                            },
				{ "rp",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "rt",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "rtc",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "ruby",       ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "s",          ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "samp",       ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "small",      ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "span",       ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "strike",     ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "strong",     ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "sub",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "summary",    ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "sup",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "time",       ElemType::NonVoid, EmbedAction::Allow, c_time_attrInfo                         },
				{ "tt",         ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "u",          ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "var",        ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "wbr",        ElemType::Void,    EmbedAction::Allow, nullptr                                 },

				// Media

				{ "embed",      ElemType::Void,    EmbedAction::Omit                                           }, // Not embeddable, unsafe
				{ "iframe",     ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, contained HTML should not use nested browsing context
				{ "noframes",   ElemType::NonVoid, EmbedAction::Omit                                           }, // Content is embedded, but not <noframes> tag itself
				{ "object",     ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, unsafe
				{ "param",      ElemType::Void,    EmbedAction::Omit                                           }, // Not embeddable, unsafe

				{ "area",       ElemType::Void,    EmbedAction::Allow, c_area_attrInfo,        area_Finalizer  },
				{ "audio",      ElemType::NonVoid, EmbedAction::Allow, c_audio_attrInfo                        },
				{ "img",        ElemType::Void,    EmbedAction::Allow, c_img_attrInfo                          },
				{ "map",        ElemType::NonVoid, EmbedAction::Allow, c_map_attrInfo                          },
				{ "picture",    ElemType::Void,    EmbedAction::Allow, nullptr                                 },
				{ "source",     ElemType::Void,    EmbedAction::Allow, c_source_attrInfo                       },
				{ "track",      ElemType::Void,    EmbedAction::Allow, c_track_attrInfo                        },
				{ "video",      ElemType::NonVoid, EmbedAction::Allow, c_video_attrInfo                        },

				// Scripting

				{ "canvas",     ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, requires scripting, which is unsafe
				{ "noscript",   ElemType::NonVoid, EmbedAction::Omit                                           }, // Content is embedded, but not <noscript> tag itself
				{ "script",     ElemType::NonVoid, EmbedAction::Omit                                           }, // Not embeddable, unsafe

				{ "template",   ElemType::NonVoid, EmbedAction::Allow, nullptr                                 }, // Requires script to use; must embed tag so its contents aren't displayed

				// Tables

				{ "caption",    ElemType::NonVoid, EmbedAction::Allow, c_caption_attrInfo                      },
				{ "col",        ElemType::Void,    EmbedAction::Allow, c_col_colgroup_attrInfo                 },
				{ "colgroup",   ElemType::NonVoid, EmbedAction::Allow, c_col_colgroup_attrInfo                 },
				{ "table",      ElemType::NonVoid, EmbedAction::Allow, c_table_attrInfo                        },
				{ "tbody",      ElemType::NonVoid, EmbedAction::Allow, c_tr_tbody_tfoot_thead_attrInfo         },
				{ "td",         ElemType::NonVoid, EmbedAction::Allow, c_td_th_attrInfo                        },
				{ "tfoot",      ElemType::NonVoid, EmbedAction::Allow, c_tr_tbody_tfoot_thead_attrInfo         },
				{ "th",         ElemType::NonVoid, EmbedAction::Allow, c_td_th_attrInfo                        },
				{ "thead",      ElemType::NonVoid, EmbedAction::Allow, c_tr_tbody_tfoot_thead_attrInfo         },
				{ "tr",         ElemType::NonVoid, EmbedAction::Allow, c_tr_tbody_tfoot_thead_attrInfo         },

				// Forms

				{ "button",     ElemType::NonVoid, EmbedAction::Allow, c_button_attrInfo                       },
				{ "datalist",   ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "fieldset",   ElemType::NonVoid, EmbedAction::Allow, c_fieldset_attrInfo                     },
				{ "form",       ElemType::NonVoid, EmbedAction::Allow, c_form_attrInfo,        form_Finalizer  },
				{ "input",      ElemType::Void,    EmbedAction::Allow, c_input_attrInfo,       input_Finalizer },
				{ "label",      ElemType::NonVoid, EmbedAction::Allow, c_label_attrInfo                        },
				{ "legend",     ElemType::NonVoid, EmbedAction::Allow, nullptr                                 },
				{ "meter",      ElemType::NonVoid, EmbedAction::Allow, c_meter_attrInfo                        },
				{ "optgroup",   ElemType::NonVoid, EmbedAction::Allow, c_optgroup_attrInfo                     },
				{ "option",     ElemType::NonVoid, EmbedAction::Allow, c_option_attrInfo                       },
				{ "output",     ElemType::NonVoid, EmbedAction::Allow, c_output_attrInfo                       },
				{ "progress",   ElemType::NonVoid, EmbedAction::Allow, c_progress_attrInfo                     },
				{ "select",     ElemType::NonVoid, EmbedAction::Allow, c_select_attrInfo                       },
				{ "textarea",   ElemType::NonVoid, EmbedAction::Allow, c_textarea_attrInfo                     },

				{ nullptr                                                                                      },
			};


		AttrInfo const* ElemInfo::FindAttrInfo_ByTagExact(Seq tagLower) const
		{
			for (AttrInfo const* ai=m_attrs; ai->m_tag; ++ai)
				if (tagLower.EqualExact(ai->m_tag))
					return ai;

			for (AttrInfo const* ai=c_globalAttrInfo; ai->m_tag; ++ai)
				if (tagLower.EqualExact(ai->m_tag))
					return ai;

			return nullptr;
		}


		ElemInfo const* FindElemInfo_ByTagExact(Seq tagLower)
		{
			for (ElemInfo const* ei=Html::c_elemInfo; ei->m_tag; ++ei)
				if (tagLower.EqualExact(ei->m_tag))
					return ei;

			return nullptr;
		}

	}
}
