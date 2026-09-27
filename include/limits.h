/*

   Copyright 2009 Pierre KRIEGER

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
   
*/

#ifndef _STDC_LIMITS_H_
#define _STDC_LIMITS_H_

#include <machine/_sizes.h>

#define PATH_MAX	260

#define CHAR_BIT 8
#define SCHAR_MIN (-128)
#define SCHAR_MAX 127
#define UCHAR_MAX 0xff

#ifdef __CHAR_UNSIGNED__
#define CHAR_MIN 0
#define CHAR_MAX UCHAR_MAX
#else
#define CHAR_MIN SCHAR_MIN
#define CHAR_MAX SCHAR_MAX
#endif

#define MB_LEN_MAX (5)
#define SHRT_MIN (-32768)
#define SHRT_MAX (32767)
#define USHRT_MAX (0xffffU)
#define INT_MIN (-2147483647 - 1)
#define INT_MAX (2147483647)
#define UINT_MAX (0xffffffffU)
#define LONG_MIN (-2147483647L - 1)
#define LONG_MAX (2147483647L)
#define ULONG_MIN (0x0)
#define ULONG_MAX (0xffffffffUL)
#define LLONG_MAX (9223372036854775807ll)
#define LLONG_MIN (-9223372036854775807ll - 1)
#define ULLONG_MAX (0xffffffffffffffffull)

#define _I8_MIN (-127 - 1)
#define _I8_MAX 127
#define _UI8_MAX 0xffu

#define _I16_MIN (-32767 - 1)
#define _I16_MAX 32767
#define _UI16_MAX 0xffffu

#define _I32_MIN (-2147483647 - 1)
#define _I32_MAX 2147483647
#define _UI32_MAX 0xffffffffu

#if defined(__GNUC__)
#undef LONG_LONG_MAX
#define LONG_LONG_MAX 9223372036854775807ll
#undef LONG_LONG_MIN
#define LONG_LONG_MIN (-LONG_LONG_MAX-1)
#undef ULONG_LONG_MAX
#define ULONG_LONG_MAX (2ull * LONG_LONG_MAX + 1ull)
#endif

#define _I64_MIN (-9223372036854775807ll - 1)
#define _I64_MAX 9223372036854775807ll
#define _UI64_MAX 0xffffffffffffffffull

#ifndef SIZE_MAX
#ifdef _WIN64
#define SIZE_MAX _UI64_MAX
#else
#define SIZE_MAX UINT_MAX
#endif
#endif

#ifndef SSIZE_MAX
#ifdef _WIN64
#define SSIZE_MAX _I64_MAX
#else
#define SSIZE_MAX INT_MAX
#endif
#endif

#endif

