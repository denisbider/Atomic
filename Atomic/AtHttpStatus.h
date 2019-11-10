#pragma once

#include "AtSeq.h"

namespace At
{
	struct HttpStatus
	{
		enum E
		{
			Continue						= 100,
			SwitchingProtocols				= 101,

			OK								= 200,
			Created							= 201,
			Accepted						= 202,
			NonAuthoritativeInformation		= 203,
			NoContent						= 204,
			ResetContent					= 205,
			PartialContent					= 206,
		
			MultipleChoices					= 300,
			MovedPermanently				= 301,
			Found							= 302,
			SeeOther						= 303,
			NotModified						= 304,
			UseProxy						= 305,
			TemporaryRedirect				= 307,
		
			BadRequest						= 400,
			Unauthorized					= 401,
			Forbidden						= 403,
			NotFound						= 404,
			MethodNotAllowed				= 405,
			NotAcceptable					= 406,
			ProxyAuthenticationRequired		= 407,
			RequestTimeout					= 408,
			Conflict						= 409,
			Gone							= 410,
			LengthRequired					= 411,
			PreconditionFailed				= 412,
			RequestEntityTooLarge			= 413,
			RequestUriTooLong				= 414,
			UnsupportedMediaType			= 415,
			RequestedRangeNotSatisfiable	= 416,
			ExpectationFailed				= 417,
			TooManyRequests					= 429,
		
			InternalServerError				= 500,
			NotImplemented					= 501,
			BadGateway						= 502,
			ServiceUnavailable				= 503,
			GatewayTimeout					= 504,
			HttpVersionNotSupported			= 505,
		};
	
		static Seq Describe(uint64 n);
	};
}
