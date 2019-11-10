#pragma once

// Copyright (C) 2018-2019 by denis bider. All rights reserved.

// Disable off-by-default warnings that should remain off. Many come from including the standard library and Windows headers
#pragma warning (disable: 4514)		// unreferenced inline function has been removed
#pragma warning (disable: 4625)		// copy constructor was implicitly defined as deleted
#pragma warning (disable: 4626)		// assignment operator was implicitly defined as deleted
#pragma warning (disable: 4668)		// not defined as a preprocessor macro
#pragma warning (disable: 4710)		// function not inlined
#pragma warning (disable: 4711)		// function selected for automatic inline expansion
#pragma warning (disable: 4820)		// padding added after data member


// Originator
#include "Originator.h"

// Windows
#include <objbase.h>

// std
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <conio.h>
#include <time.h>
