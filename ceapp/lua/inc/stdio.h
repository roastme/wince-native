/* The CE SDK stdio.h needs the Windows types first; wceex.h declares the helpers luace.h maps to. */
#include <windows.h>
#include_next <stdio.h>
#include "wceex.h"
