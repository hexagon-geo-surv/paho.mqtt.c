#pragma once

// This requires Visual Studio 2010 on Windows, all other platforms should have had the standard C99 stdint.h for ages now
#if defined(_MSC_VER) && (_MSC_VER < 1600)

typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;

#else
#include <stdint.h>
#endif
