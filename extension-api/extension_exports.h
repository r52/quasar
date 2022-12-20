#pragma once

#ifdef extension_EXPORTS
#  define extension_API __declspec(dllexport)
#else
#  define extension_API __declspec(dllimport)
#endif
