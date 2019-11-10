#include "SpkIncludes.h"


void EncodeAsZLit(Enc& enc, Seq content)
{
	enum { NewLineThresholdBytes = 180 };
	sizet curLineBytes {};

	enc.ReserveInc(2 + (4 * content.n));
	enc.Ch('"');

	auto mustEscapeByte = [] (uint c) { return c<=31 || c>=127 || c=='"' || c=='\\'; };
	Seq toEscape;
	while (true)
	{
		bool needNewLine {};
		if (!toEscape.n)
		{
			sizet maxToEndOfLine = NewLineThresholdBytes - curLineBytes;
			Seq nonEscapedBytes = content.ReadToFirstByteOfType(mustEscapeByte, maxToEndOfLine);
			enc.Add(nonEscapedBytes);
			curLineBytes += nonEscapedBytes.n;
		}
		else
		{
			while (true)
			{
				uint b = toEscape.FirstByte();
				bool escShort = (b==0 || b=='\t' || b=='\n' || b=='\r' || b=='"' || b=='\\');
				sizet escBytes = (escShort ? 2U : 4U);

				if (curLineBytes + escBytes > NewLineThresholdBytes)
				{
					needNewLine = true;
					break;
				}

				     if (!escShort) enc.Add("\\x").Hex(b);
				else if (b ==    0) enc.Add("\\0");
				else if (b == '\t') enc.Add("\\t");
				else if (b == '\n') enc.Add("\\n");
				else if (b == '\r') enc.Add("\\r");
				else if (b ==  '"') enc.Add("\\\"");
				else if (b == '\\') enc.Add("\\\\");
				else EnsureThrow(!"Unexpected value to escape");

				toEscape.DropByte();
				curLineBytes += escBytes;
				if (!toEscape.n)
					break;
			}
		}

		if (needNewLine || curLineBytes >= NewLineThresholdBytes)
		{
			enc.Add("\"\r\n\t\"");
			curLineBytes = 0;
		}

		if (!toEscape.n)
		{
			uint b = content.FirstByte();
			if (b == UINT_MAX)
				break;

			if (mustEscapeByte(b))
				toEscape = content.ReadUtf8_MaxChars(1);
		}
	}
	
	enc.Ch('"');
}


enum class InFileType { Undefined, NoPack, Js, Css };

struct InFileInfo
{
	Str        m_path;
	PathParts  m_pathParts;
	InFileType m_type;
	File       m_file;
};


int main(int argc, char** argv)
{
	try
	{
		// If no parameters, display help and exit
		if (argc < 2)
		{
			Console::Out(
				"ScriptPack, a very simple packer to embed JS and CSS resources in C++ programs\r\n"
				"Copyright (C) 2019 by denis bider. All rights reserved.\r\n"
				"\r\n"
				"Usage:\r\n"
				"  ScriptPack -out <outPathBase> -in <inFile1> ... [-np <npFile1> ...]\r\n"
				"             [-pch <pch>] [-incl <hdr1> ...] [-ns <namespace>]\r\n"
				"\r\n"
				"outPathBase  - base name for two output .h/.cpp files; overwritten if existing\r\n"
				"inFile1, ... - relative or absolute paths to JS or CSS files. Wildcards OK\r\n"
				"npFile1, ... - like -in, but content not packed before output. Wildcards OK\r\n"
				"[pch]        - precompiled header name to include in generated .h and .cpp files\r\n"
				"[hdr1, ...]  - additional header names to include in the generated .h file\r\n"
				"[namespace]  - optional name of namespace in which to put the generated strings\r\n"
				"\r\n"
				"Transformations performed:\r\n"
				"- comments are removed from output\r\n"
				"- whitespace sequences outside of strings are collapsed into a single space\r\n"
				"- result format is one C/C++ string literal for each input file\r\n"
				"\r\n"
				"Limitations:\r\n"
				"- names of identifiers are left unchanged\r\n"
				"- JS regular expressions will cause parsing errors or incorrect output\r\n"
				"- code relying on semicolon insertion will break since newlines are removed\r\n"
				"- each packed input file must be below max string literal size (64k for MSVC)\r\n");
			return 2;
		}

		// Parse parameters
		enum class ArgMode { Undefined, Out, In, Np, Pch, Incl, Ns } argMode {};
		Seq outPathBase;
		Vec<InFileInfo> inFiles;
		Seq pch;
		Vec<Seq> includeHdrs;
		Seq ns;

		for (int i=1; i!=argc; ++i)
		{
			Seq arg = argv[i];
			if (arg.EqualInsensitive("-out"))
				argMode = ArgMode::Out;
			else if (arg.EqualInsensitive("-in"))
				argMode = ArgMode::In;
			else if (arg.EqualInsensitive("-np"))
				argMode = ArgMode::Np;
			else if (arg.EqualInsensitive("-pch"))
				argMode = ArgMode::Pch;
			else if (arg.EqualInsensitive("-incl"))
				argMode = ArgMode::Incl;
			else if (arg.EqualInsensitive("-ns"))
				argMode = ArgMode::Ns;
			else
			{
				if (argMode == ArgMode::Out)
				{
					if (outPathBase.Any())
						throw Str::Join("Output path base already defined at argument:\r\n", arg);
					
					if (!arg.Any())
						throw Str("Output path base cannot be empty");

					if (arg.EndsWithExact("\\") || arg.EndsWithExact("/") || arg.EndsWithExact("."))
						throw Str::Join("Output path base ends in an invalid character:\r\n", arg);

					outPathBase = arg;
				}
				else if (argMode == ArgMode::In || argMode == ArgMode::Np)
				{
					if (!arg.Any())
						throw Str("Input file path cannot be empty");

					auto addInFile = [&] (Seq path)
						{
							InFileInfo& ifi = inFiles.Add();
							ifi.m_path = path;

								 if (argMode == ArgMode::Np)          ifi.m_type = InFileType::NoPack;
							else if (arg.EndsWithInsensitive(".js"))  ifi.m_type = InFileType::Js;
							else if (arg.EndsWithInsensitive(".css")) ifi.m_type = InFileType::Css;
							else throw Str::Join("Unrecognized input file extension - must be .js or .css:\r\n", arg);
						};

					if (!arg.ContainsAnyByteOf("*?"))
						addInFile(arg);
					else
					{
						PathParts parts { arg };
						FindFiles ff { arg };
						while (ff.Next())
						{
							FindFiles::Result const& fr = ff.Current();
							addInFile(JoinPath(parts.m_dir, fr.m_fileName));
						}
					}
				}
				else if (argMode == ArgMode::Pch)
				{
					if (pch.Any())
						throw Str::Join("Precompiled header name already defined at argument:\r\n", arg);

					if (!arg.Any())
						throw Str("Precompiled header name cannot be empty");

					pch = arg;
				}
				else if (argMode == ArgMode::Incl)
				{
					if (!arg.Any())
						throw Str("Include header name cannot be empty");

					includeHdrs.Add(arg);
				}
				else if (argMode == ArgMode::Ns)
				{
					if (ns.Any())
						throw Str::Join("Namespace name already defined at argument:\r\n", arg);

					if (!arg.Any())
						throw Str("Namespace name cannot be empty");

					ns = arg;
				}
				else
					throw Str::Join("Unrecognized argument type:\r\n", arg);
			}
		}

		if (!inFiles.Any())
			throw Str("No input files provided or found");

		if (!outPathBase.Any())
			throw Str("No output path base provided");

		// Get our own executable modification time
		Time lastInWriteTime;

		Str selfModulePath = GetModulePath();
		File::AttrsEx attrsSelf;
		if (!File::GetAttrsEx(selfModulePath, attrsSelf))
			throw WinErr<>(attrsSelf.m_err, "Could not obtain own executable modification time: Error in File::GetAttrsEx");

		lastInWriteTime = attrsSelf.m_times.m_lastWriteTime;

		// Open input files
		for (InFileInfo& ifi : inFiles)
		{
			ifi.m_pathParts.Parse(ifi.m_path);
			ifi.m_file.Open(ifi.m_path, File::OpenArgs::DefaultRead());

			Time t = ifi.m_file.GetTimes().m_lastWriteTime;
			if (lastInWriteTime < t)
				lastInWriteTime = t;
		}

		// Are output files newer than input files?
		Str outFileNameHdr, outFileNameCpp;
		outFileNameHdr.SetAdd(outPathBase, ".h");
		outFileNameCpp.SetAdd(outPathBase, ".cpp");

		File::AttrsEx attrsHdr, attrsCpp;
		if (File::GetAttrsEx(outFileNameHdr, attrsHdr) && attrsHdr.m_times.m_lastWriteTime >= lastInWriteTime &&
			File::GetAttrsEx(outFileNameCpp, attrsCpp) && attrsCpp.m_times.m_lastWriteTime >= lastInWriteTime)
		{
			Console::Out("No scripts packed: all output files are newer than input files\r\n");
			return 0;
		}

		// Create or truncate output files
		File outFileHdr, outFileCpp;
		outFileHdr.Open(outFileNameHdr, File::OpenArgs::DefaultOverwrite());
		outFileCpp.Open(outFileNameCpp, File::OpenArgs::DefaultOverwrite());

		// Write output header file preamble
		outFileHdr.Write(
			"// File generated by ScriptPack. Overwritten in build. Do not edit manually\r\n\r\n"
			"#pragma once\r\n\r\n");

		if (pch.Any())
			outFileHdr.Write(Str::Join("#include \"", pch, "\"\r\n"));
		for (Seq hdr : includeHdrs)
			outFileHdr.Write(Str::Join("#include \"", hdr, "\"\r\n"));

		if (pch.Any() || includeHdrs.Any())
			outFileHdr.Write("\r\n\r\n");

		// Write output source file preamble
		outFileCpp.Write("// File generated by ScriptPack. Overwritten in build. Do not edit manually\r\n\r\n");

		if (pch.Any())
			outFileCpp.Write(Str::Join("#include \"", pch, "\"\r\n"));

		PathParts outPathParts { outPathBase };
		outFileCpp.Write(Str::Join("#include \"", outPathParts.m_baseName, ".h\"\r\n\r\n\r\n"));

		// Write namespace, if any
		if (ns.Any())
		{
			Str nsPreamble;
			nsPreamble.SetAdd("namespace ", ns, "\r\n{\r\n\r\n");
			outFileHdr.Write(nsPreamble);
			outFileCpp.Write(nsPreamble);
		}

		// Process input files
		for (InFileInfo& ifi : inFiles)
		{
			char const* declPrefix {};
			char const* showDesc   {};
				 if (ifi.m_type == InFileType::NoPack) { declPrefix = "c_nopack_"; showDesc = " (no pack)"; }
			else if (ifi.m_type == InFileType::Js)     { declPrefix = "c_js_";     showDesc = " (js)";      }
			else if (ifi.m_type == InFileType::Css)    { declPrefix = "c_css_";    showDesc = " (css)";     }
			else EnsureThrow(!"Unrecognized InFileType");

			Console::Out(Str::Join(ifi.m_path, showDesc, "\r\n"));

			try
			{
				Str declName;
				declName.SetAdd(declPrefix, ifi.m_pathParts.m_baseName);

				outFileHdr.Write(Str::Join("extern Seq ", declName, ";\r\n"));
				outFileCpp.Write(Str::Join("Seq ", declName, " {\r\n\t"));

				Str inContent;
				ifi.m_file.ReadAllInto(inContent);

				Str contentPacked;
				Seq contentToEncode;

				if (ifi.m_type == InFileType::NoPack)
					contentToEncode = inContent;
				else
				{
					if (ifi.m_type == InFileType::Js)
						JsWsPack(contentPacked, inContent);
					else if (ifi.m_type == InFileType::Css)
						CssPack(contentPacked, inContent);
					else
						EnsureThrow(!"Unrecognized InFileType");

					contentToEncode = Seq(contentPacked).Trim();
				}

				Str zLit;
				EncodeAsZLit(zLit, contentToEncode);

				outFileCpp.Write(Str::Join(zLit, ",\r\n\t").UInt(contentToEncode.n).Add(" };\r\n\r\n"));
			}
			catch (Str const& s)
			{
				Console::Out(s);
				return 1;
			}
		}

		// Write end of namespace, if any
		if (ns.Any())
		{
			outFileHdr.Write("\r\n}\r\n");
			outFileCpp.Write("}\r\n");
		}

		return 0;
	}
	catch (Str const& s)
	{
		Console::Out(Str::Join(Seq(s).Trim(), "\r\nRun program without parameters for help\r\n"));
		return 2;
	}
	catch (std::exception const& e)
	{
		Console::Out(Str::Join("ScriptPack terminated by exception:\r\n", Seq(e.what()).Trim(), "\r\n"));
		return 1;
	}
}
