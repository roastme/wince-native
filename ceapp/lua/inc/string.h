/* The CE SDK string.h needs the Windows types first; wceex.h declares the helpers luace.h maps to. */
#include <windows.h>
#include_next <string.h>
#include "wceex.h"
