#include "AtIncludes.h"
#include "AtHttpVerb.h"

namespace At
{
	Seq HttpVerb::Describe(uint64 n)
	{
		switch (n)
		{
		case HttpVerbUnparsed:	return "(Unparsed HTTP Verb)";
		case HttpVerbUnknown:	return "(Unknown HTTP Verb)";
		case HttpVerbInvalid:	return "(Invalid HTTP Verb)";
		case HttpVerbOPTIONS:	return "OPTIONS";
		case HttpVerbGET:		return "GET";
		case HttpVerbHEAD:		return "HEAD";
		case HttpVerbPOST:		return "POST";
		case HttpVerbPUT:		return "PUT";
		case HttpVerbDELETE:	return "DELETE";
		case HttpVerbTRACE:		return "TRACE";
		case HttpVerbCONNECT:	return "CONNECT";
		case HttpVerbTRACK:		return "TRACK";
		case HttpVerbMOVE:		return "MOVE";
		case HttpVerbCOPY:		return "COPY";
		case HttpVerbPROPFIND:	return "PROPFIND";
		case HttpVerbPROPPATCH:	return "PROPPATCH";
		case HttpVerbMKCOL:		return "MKCOL";
		case HttpVerbLOCK:		return "LOCK";
		case HttpVerbUNLOCK:	return "UNLOCK";
		case HttpVerbSEARCH:	return "SEARCH";
		default:				return "(Unrecognized HTTP Verb)";
		}
	}
}
