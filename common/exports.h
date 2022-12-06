#pragma once

#ifdef common_EXPORTS
#  define common_API __declspec(dllexport)
#else
#  define common_API __declspec(dllimport)
#endif
