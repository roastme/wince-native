/* The CE SDK stdlib.h needs the Windows types first; wceex.h declares the helpers luace.h maps to. */
#include <windows.h>
#include_next <stdlib.h>
#include "wceex.h"
