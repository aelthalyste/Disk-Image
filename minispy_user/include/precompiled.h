/* date = April 16th 2021 4:34 pm */
#pragma once

#include <stdio.h>
#include <string>
#include <string.h>
#include <stdint.h>
#include <vector>

#include <iostream>


#include <functional>
#include <fstream>
#include <streambuf>
#include <sstream>


// WIN32 LAYER
#if _WIN32
#include <windows.h>
#include <winioctl.h>
#include <emmintrin.h>
#include <memory.h>

#include <atlbase.h>

#include <vds.h>
#include <vss.h>
#include <vswriter.h>
#include <vsbackup.h>
#include <vsmgmt.h>
#include <fltUser.h>
#include <intrin.h>


#include "iphlpapi.h"

#include "minispy.h"
#endif
