#include "AtIncludes.h"
#include "AtHttpStatus.h"

namespace At
{
	Seq HttpStatus::Describe(uint64 n)
	{
		switch (n)
		{
		case Continue:						return "Continue";
		case SwitchingProtocols:			return "Switching Protocols";

		case OK:							return "OK";
		case Created:						return "Created";
		case Accepted:						return "Accepted";
		case NonAuthoritativeInformation:	return "Non-Authoritative Information";
		case NoContent:						return "No Content";
		case ResetContent:					return "Reset Content";
		case PartialContent:				return "Partial Content";

		case MultipleChoices:				return "Multiple Choices";
		case MovedPermanently:				return "Moved Permanently";
		case Found:							return "Found";
		case SeeOther:						return "See Other";
		case NotModified:					return "Not Modified";
		case UseProxy:						return "Use Proxy";
		case TemporaryRedirect:				return "Temporary Redirect";

		case BadRequest:					return "BadRequest";
		case Unauthorized:					return "Unauthorized";
		case Forbidden:						return "Forbidden";
		case NotFound:						return "Not Found";
		case MethodNotAllowed:				return "Method Not Allowed";
		case NotAcceptable:					return "Not Acceptable";
		case ProxyAuthenticationRequired:	return "Proxy Authentication Required";
		case RequestTimeout:				return "Request Timeout";
		case Conflict:						return "Conflict";
		case Gone:							return "Gone";
		case LengthRequired:				return "Length Required";
		case PreconditionFailed:			return "Precondition Failed";
		case RequestEntityTooLarge:			return "Request Entity Too Large";
		case RequestUriTooLong:				return "Request URI Too Long";
		case UnsupportedMediaType:			return "Unsupported Media Type";
		case RequestedRangeNotSatisfiable:	return "Requested Range Not Satisfiable";
		case ExpectationFailed:				return "Expectation Failed";

		case InternalServerError:			return "Internal Server Error";
		case NotImplemented:				return "Not Implemented";
		case BadGateway:					return "Bad Gateway";
		case ServiceUnavailable:			return "Service Unavailable";
		case GatewayTimeout:				return "Gateway Timeout";
		case HttpVersionNotSupported:		return "HTTP Version Not Supported";
	
		default:							return "(Unrecognized HTTP Status Code)";
		}
	}
}
