#pragma once

#define __STR2__(x) #x
#define __STR__(x) __STR2__(x)

#if defined(__INTEL_COMPILER)
#define COMPILER_STRING "Intel C++ " __STR__(__VERSION__)
#elif defined(__clang__)
#define COMPILER_STRING "clang " __STR__(__clang_version__)
#elif defined(__GNUC__)
#define COMPILER_STRING "g++ " __STR__(__VERSION__)
#elif defined(_MSC_VER)
#define COMPILER_STRING __STR__(__VERSION__)
#endif