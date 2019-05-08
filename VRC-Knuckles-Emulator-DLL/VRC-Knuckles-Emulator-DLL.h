#pragma once

#ifdef VRCKNUCKLESEMULATORDLL_EXPORTS
#define VRCKNUCKLESEMULATORDLL_API extern "C" __declspec(dllexport)
#else
#define VRCKNUCKLESEMULATORDLL_API extern "C" __declspec(dllimport)
#endif
