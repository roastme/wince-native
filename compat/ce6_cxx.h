/* Force-included by chk6.sh only (C++ syntax checks of CE 6 shell sources): stand-ins for MSVC-only C++ in the SDK headers. */
#ifndef CE6_CXX_H
#define CE6_CXX_H
#define throw(x)    /* MSVC dynamic exception specs (throw(...)) are not C++ */
#define _Complex    _complex_
#define wcsicmp     _wcsicmp
#define wcslwr      _wcslwr
#endif
