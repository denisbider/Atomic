#pragma once

#include "AtStr.h"

namespace At
{
	// Allows for a client and a server to generate the same unique BHTPN pipe name dependent on a shared base name.
	// The client and the server executables need to be placed into the same directory. The base name of the client
	// executable should equal the base name of the service executable, plus "Admin". All paths are case insensitive.
	//
	// If no module path is provided, uses GetModulePath().
	//
	// Example:
	//
	// BHTPN server executable is at "C:\Path\Server.exe".
	// BHTPN client executable is at "C:\Path\ServerAdmin.exe".
	// BHTPN pipe name is generated as a truncated hash of "c:\path\server".

	Str Gen_ModulePathBased_BhtpnPipeName(Seq modulePath = Seq());
}
