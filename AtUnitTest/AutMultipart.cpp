#include "AutIncludes.h"
#include "AutMain.h"


void MultipartTest(Seq encoded)
{
	Console::Out(Str("\r\n").Add(encoded).Add("\r\n"));

	ParseTree pt { encoded };
	pt.RecordBestToStack();
	if (!pt.Parse(Mime::C_multipart_body))
	{
		Console::Out(Str("Error parsing multipart body:\r\n")
					.Obj(pt, ParseTree::BestAttempt).Add("\r\n"));
	}
	else
	{
		Console::Out("Parsed\r\n");

		Mime::MultipartBody mltp;
		PinStore store { 4000 };
		if (!mltp.Read(encoded, store))
			Console::Out("MultipartBody could not be read\r\n");
		else
			Console::Out(Str().UInt(mltp.m_parts.Len()).Add(" body parts read\r\n"));
	}
}


void MultipartTests()
{
	MultipartTest(
		"------WebKitFormBoundary45kzLDMMQQb3ywPr\r\n"
		"Content-Disposition: form-data; name=\"PostForm\"\r\n"
		"\r\n"
		"RYmu0FR6xDEvJ7JzGGMT516Z\r\n"
		"------WebKitFormBoundary45kzLDMMQQb3ywPr--\r\n");

	MultipartTest(
		"------WebKitFormBoundary45kzLDMMQQb3ywPr\r\n"
		"Content-Disposition: form-data; name=\"PostForm\"\r\n"
		"\r\n"
		"RYmu0FR6xDEvJ7JzGGMT516Z\r\n"
		"------WebKitFormBoundary45kzLDMMQQb3ywPr\r\n"
		"Content-Disposition: form-data; name=\"cmd\"\r\n"
		"\r\n"
		"Import\r\n"
		"------WebKitFormBoundary45kzLDMMQQb3ywPr--\r\n");

	MultipartTest(
		"------WebKitFormBoundary45kzLDMMQQb3ywPr\r\n"
		"Content-Disposition: form-data; name=\"PostForm\"\r\n"
		"\r\n"
		"RYmu0FR6xDEvJ7JzGGMT516Z\r\n"
		"------WebKitFormBoundary45kzLDMMQQb3ywPr\r\n"
		"Content-Disposition: form-data; name=\"file\"; filename=\"emails.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"xy@xavozbadiz.com\r\n"
		"xavoz.badiz3@gentron.com\r\n"
		"\r\n"
		"------WebKitFormBoundary45kzLDMMQQb3ywPr\r\n"
		"Content-Disposition: form-data; name=\"cmd\"\r\n"
		"\r\n"
		"Import\r\n"
		"------WebKitFormBoundary45kzLDMMQQb3ywPr--\r\n");
}
