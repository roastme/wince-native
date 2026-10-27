/* Force-included after compat/ms_compat.h for CE applications: SDK header inline helpers become weak, so every object may carry one. */
#undef _inline
#define _inline __attribute__((weak))
