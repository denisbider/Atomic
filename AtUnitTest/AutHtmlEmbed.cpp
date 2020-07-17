#include "AutIncludes.h"
#include "AutMain.h"


struct RefPart
{
	Str m_cid;
	Str m_fileName;
	Str m_url;

	RefPart(Seq cid) : m_cid(cid) {}
	Str const& Key() const { return m_cid; }
};

using RefPartMap = Map<RefPart>;


struct AutEmbedCx : Html::EmbedCx
{
	Str        m_outBaseName;
	RefPartMap m_refPartMap;
	sizet      m_nextRefPartNr {};

	AutEmbedCx(PinStore& store, Seq outBaseName) : Html::EmbedCx(store), m_outBaseName(outBaseName) {}
	Seq GetUrlForCid(Seq cid) override final;
};


Seq AutEmbedCx::GetUrlForCid(Seq cid)
{
	bool added {};
	RefPartMap::It it = m_refPartMap.FindOrAdd(added, cid);
	if (added)
	{
		EnsureThrow(!it->m_fileName.Any());
		it->m_fileName.UInt(m_nextRefPartNr++, 10, 3);
		it->m_url.SetAdd(m_outBaseName, "/", it->m_fileName);
	}

	return it->m_url;
}


void HtmlEmbedTest(Args& args)
{
	if (!args.Any())
	{
		throw Args::Err(
			"Parameters:\r\n"
			"\r\n"
			"-in <inFilesPattern>\r\n"
			"  Required. Directory and search pattern for emails to be processed.\r\n"
			"  Matches must be one email per file. Thunderbird \"From -\" first line\r\n"
			"  is tolerated. Input files are NOT modified.\r\n"
			"\r\n"
			"-out <dir>\r\n"
			"  Optional. Output directory under which to generate output.\r\n"
			"  If not provided, current directory is used.\r\n"
			"\r\n"
			"-dhp\r\n"
			"  Optional. Dump HTML parse tree before embedding.\r\n");
	}

	Str inFilesPattern;
	Str outDir;
	bool dumpHtmlParse {};

	do
	{
		Seq arg = args.Next();

		if (arg.EqualInsensitive("-in"))
			inFilesPattern = args.NextOrErr("Missing input files pattern");
		else if (arg.EqualInsensitive("-out"))
			outDir = args.NextOrErr("Missing output directory");
		else if (arg.EqualInsensitive("-dhp"))
			dumpHtmlParse = true;
		else
			throw Args::Err(Str::Join("Unrecognized argument: ", arg));
	}
	while (args.Any());

	if (!inFilesPattern.Any())
		throw Args::Err("Input files pattern not specified");

	if (!outDir.Any())
		outDir = ".";
	else if (!File::Exists_IsDirectory(outDir))
		throw Args::Err(Str::Join("Output directory \"", outDir, "\" does not exist or is not a directory"));

	Crypt::Initializer cryptInit;

	PathParts          inPathParts { inFilesPattern };
	FindFiles          inFiles     { inFilesPattern };
	bool               needNewline {};
	ParseTree::Storage ptStorage;

	sizet nrFiles {}, nrMsgParseFail {}, nrUnsupCt {}, nrMptParseFail {}, nrHtmlNotFound {}, nrHtmlDecodeFail {}, nrHtmlParseFail {}, nrPartFail {};

	while (inFiles.Next())
	{
		FindFiles::Result const& result = inFiles.Current();
		if (!result.IsDirectory())
		{
			++nrFiles;

			if (nrFiles == 1) { Console::Out("Processing"); needNewline = true; }
			else if ((nrFiles % 100) == 1) { Console::Out("."); needNewline = true; }

			Str inFilePath = JoinPath(inPathParts.m_dir, result.m_fileName);
			Str inFileContent;
			File().Open(inFilePath, File::OpenArgs::DefaultRead()).ReadAllInto(inFileContent);
			Seq contentReader = inFileContent;

			// Thunderbird: Skip leading "From - <date>" line
			if (contentReader.StartsWithExact("From - "))
				contentReader.DropToFirstByteOf("\r\n").DropToFirstByteNotOf("\r\n");

			// Try to parse
			ParseTree ptMsg { contentReader, &ptStorage };
			if (!ptMsg.Parse(Imf::C_message))
			{
				if (needNewline) { Console::Out("\r\n"); needNewline = false; }
				Console::Err(Str(result.m_fileName).Add(": msg: ").Obj(ptMsg, ParseTree::BestAttempt).Add("\r\n"));
				++nrMsgParseFail;
			}
			else
			{
				Imf::Message msg;
				PinStore pinStore { contentReader.n };
				msg.Read(ptMsg, pinStore);

				Mime::PartReadCx prcx;

				if (!msg.m_htmlContentPart)
				{
					if (!msg.IsMultipart())
					{
						if (needNewline) { Console::Out("\r\n"); needNewline = false; }
						Console::Err(Str(result.m_fileName).Add(": unsupported message content type\r\n"));
						++nrUnsupCt;
					}

					// RFC 2045 mandates an identity encoding ("7bit", "8bit" or "binary") MUST be used
					if (!msg.ReadMultipartBody(pinStore, prcx) || prcx.m_errs.Any())
					{
						if (needNewline) { Console::Out("\r\n"); needNewline = false; }
						Console::Err(Str(result.m_fileName).Add(": MIME: ").Obj(prcx).Add("\r\n"));
						++nrMptParseFail;
					}
					else if (!msg.m_htmlContentPart)
					{
						if (needNewline) { Console::Out("\r\n"); needNewline = false; }
						Console::Err(Str(result.m_fileName).Add(": HTML content part not found\r\n"));
						++nrHtmlNotFound;
					}
				}

				if (msg.m_htmlContentPart)
				{
					Mime::PartContent htmlPartContent { *msg.m_htmlContentPart, pinStore };
					if (!htmlPartContent.m_success)
					{
						if (needNewline) { Console::Out("\r\n"); needNewline = false; }
						Console::Err(Str(result.m_fileName).Add(": HTML content part could not be decoded\r\n"));
						++nrHtmlDecodeFail;
					}
					else
					{
						PathParts inFileNameParts { result.m_fileName };
						Seq outBaseName = inFileNameParts.m_baseName;
						if (!outBaseName.n)
							outBaseName = "msg";

						AutEmbedCx embedCx { pinStore, outBaseName };
						HtmlBuilder embedHtml { htmlPartContent.m_decoded.n };
						Html::Transform transform { htmlPartContent.m_decoded };
						if (!transform.Parse())
						{
							if (needNewline) { Console::Out("\r\n"); needNewline = false; }
							Console::Err(Str(result.m_fileName).Add(": HTML content part could not be parsed\r\n"));
							++nrHtmlParseFail;
						}
						else
						{
							if (dumpHtmlParse)
							{
								Str s;
								transform.Tree().Root().Dump(s);
								s.SetEndExact("\r\n");
								Console::Out(s);
							}

							transform.ToEmbeddableHtml(embedHtml, embedCx);
							Seq embedHtmlSeq = embedHtml.Final();
					
							Seq title = "(No subject)";
							if (msg.m_subject.Any())
								title = msg.m_subject->m_subject;

							auto renderAddrList = [] (HtmlBuilder& html, Seq desc, Imf::AddressList const& addrList)
								{
									if (addrList.m_addresses.Any())
									{
										html.Tr().Td().T(desc).EndTd().Td();

										Str addrListStr;
										Imf::MsgWriter writer { addrListStr };
										addrList.Write(writer);

										html.T(addrListStr).EndTd().EndTr();
									}
								};

							HtmlBuilder html;
							html.DefaultHead()
									.Title().T(title).EndTitle()
									.AddCss(
										"html, body { height: 100%; } "
										".frameContainer { width: 90%; height: 90%; max-width: 1000px; } "
										"iframe { width: 100%; height: 100%; }")
								.EndHead()
								.Body()
									.Table();

							if (msg.m_from .Any()) renderAddrList(html, "From", msg.m_from ->m_addrList);
							if (msg.m_to   .Any()) renderAddrList(html, "To",   msg.m_to   ->m_addrList);
							if (msg.m_cc   .Any()) renderAddrList(html, "Cc",   msg.m_cc   ->m_addrList);

							if (msg.m_subject.Any())
							{
								html.Tr()
										.Td().T("Subject").EndTd()
										.Td().T(msg.m_subject->m_subject).EndTd()
									.EndTr();
							}

							html	.EndTable()
									.Div().Class("frameContainer")
										.Iframe().Sandbox("allow-top-navigation-by-user-activation allow-same-origin").SrcDoc(embedHtmlSeq).EndIframe()
									.EndDiv()
								.EndBody()
								.EndHtml();

							Seq htmlSeq = html.Final();

							// Write main output file
							Str outFileName = JoinPath(outDir, outBaseName);
							outFileName.Add(".html");

							File().Open(outFileName, File::OpenArgs::DefaultOverwrite()).Write(Utf8::Lit::BOM).Write(htmlSeq);

							// Any parts to write?
							if (embedCx.m_refPartMap.Any())
							{
								Str subDir;
								bool subDirCreated {};
								sizet nrPartsWritten {};

								for (Mime::Part const* part : msg.m_htmlRelatedParts)
									if (part->m_contentId.Any())
									{
										RefPartMap::ConstIt it = embedCx.m_refPartMap.Find(part->m_contentId->m_id);
										if (it.Any())
										{
											Mime::PartContent partContent { *part, pinStore };
											if (partContent.m_success)
											{
												if (!subDirCreated)
												{
													subDir = JoinPath(outDir, outBaseName);
													CreateDirectoryIfNotExists(subDir, DirSecurity::SystemDefault);
													subDirCreated = true;
												}

												Str partFileName = JoinPath(subDir, it->m_fileName);
												File().Open(partFileName, File::OpenArgs::DefaultOverwrite()).Write(partContent.m_decoded);
												++nrPartsWritten;
											}
										}
									}

								if (nrPartsWritten != embedCx.m_refPartMap.Len())
								{
									if (needNewline) { Console::Out("\r\n"); needNewline = false; }
									Console::Err(Str(result.m_fileName).Add(": one or more referenced parts could not be found or decoded\r\n"));
									++nrPartFail;
								}
							}
						}
					}
				}
			}
		}
	}

	if (needNewline)
		Console::Out("\r\n");

	Console::Out(Str("\r\n").UInt(nrFiles).Add(" files processed:\r\n")
		.UInt(nrMsgParseFail)   .Add(" message parse failures\r\n")
		.UInt(nrUnsupCt)        .Add(" unsupported message content type\r\n")
		.UInt(nrMptParseFail)   .Add(" multipart parse failures\r\n")
		.UInt(nrHtmlNotFound)   .Add(" HTML content part not found\r\n")
		.UInt(nrHtmlDecodeFail) .Add(" HTML decode failures\r\n")
		.UInt(nrHtmlParseFail)  .Add(" HTML parse failures\r\n")
		.UInt(nrPartFail)       .Add(" part finding/decoding failures\r\n"));
}
