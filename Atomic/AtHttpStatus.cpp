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
		case Processing:					return "Processing";
		case EarlyHints:					return "Early Hints";

		case OK:							return "OK";
		case Created:						return "Created";
		case Accepted:						return "Accepted";
		case NonAuthoritativeInformation:	return "Non-Authoritative Information";
		case NoContent:						return "No Content";
		case ResetContent:					return "Reset Content";
		case PartialContent:				return "Partial Content";
		case MultiStatus:					return "Multi-Status";
		case AlreadyReported:				return "Already Reported";
		case IMUsed:						return "IM Used";

		case MultipleChoices:				return "Multiple Choices";
		case MovedPermanently:				return "Moved Permanently";
		case Found:							return "Found";
		case SeeOther:						return "See Other";
		case NotModified:					return "Not Modified";
		case UseProxy:						return "Use Proxy";
		case TemporaryRedirect:				return "Temporary Redirect";
		case PermanentRedirect:				return "Permanent Redirect";

		case BadRequest:					return "BadRequest";
		case Unauthorized:					return "Unauthorized";
		case PaymentRequired:				return "Payment Required";
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
		case PayloadTooLarge:				return "Payload Too Large";
		case UriTooLong:					return "URI Too Long";
		case UnsupportedMediaType:			return "Unsupported Media Type";
		case RangeNotSatisfiable:			return "Range Not Satisfiable";
		case ExpectationFailed:				return "Expectation Failed";
		case MisdirectedRequest:			return "Misdirected Request";
		case UnprocessableEntity:			return "Unprocessable Entity";
		case Locked:						return "Locked";
		case FailedDependency:				return "Failed Dependency";
		case TooEarly:						return "Too Early";
		case UpgradeRequired:				return "Upgrade Required";
		case PreconditionRequired:			return "Precondition Required";
		case TooManyRequests:				return "Too Many Requests";
		case RequestHeaderFieldsTooLarge:	return "Request Header Fields Too Large";
		case UnavailableForLegalReasons:	return "Unavailable For Legal Reasons";

		case InternalServerError:			return "Internal Server Error";
		case NotImplemented:				return "Not Implemented";
		case BadGateway:					return "Bad Gateway";
		case ServiceUnavailable:			return "Service Unavailable";
		case GatewayTimeout:				return "Gateway Timeout";
		case HttpVersionNotSupported:		return "HTTP Version Not Supported";
		case VariantAlsoNegotiates:			return "Variant Also Negotiates";
		case InsufficientStorage:			return "Insufficient Storage";
		case LoopDetected:					return "Loop Detected";
		case NotExtended:					return "Not Extended";
		case NetworkAuthenticationRequired:	return "Network Authentication Required";
	
		default:							return "(Unrecognized HTTP Status Code)";
		}
	}
}
