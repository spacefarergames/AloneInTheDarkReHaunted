#pragma once
#include <cstdlib>

#ifdef FITD_UNITY
// Unwind only the native gameplay thread; never terminate the Unity host.
struct FitdHostExit { int code; };
[[noreturn]] inline void fitdExit(int code) { throw FitdHostExit{code}; }
#else
[[noreturn]] inline void fitdExit(int code) { std::exit(code); }
#endif
