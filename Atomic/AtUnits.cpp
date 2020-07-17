#include "AtIncludes.h"
#include "AtUnits.h"


namespace At
{
	namespace Units
	{

		Unit Bytes[Bytes_NrUnits] = {
				{                            1ULL,  "B" },
				{                         1024ULL, "kB" },
				{                 1024ULL*1024ULL, "MB" },
				{         1024ULL*1024ULL*1024ULL, "GB" },
				{ 1024ULL*1024ULL*1024ULL*1024ULL, "TB" },
			};

		Unit kB[kB_NrUnits] = {
				{                            1ULL, "kB" },
				{                         1024ULL, "MB" },
				{                 1024ULL*1024ULL, "GB" },
				{         1024ULL*1024ULL*1024ULL, "TB" },
			};

	}
}
