/* Shim for syntax checks of CE 5 coredll time.c.
 * The CE 5 SDK notify.h is C++ only (string.hxx, list.hxx, sync.hxx) and does not
 * define NOTIFICATION_EVENT_TIME_CHANGE. Value 1 is taken from the CE 6 SDK notify.h. */
#ifndef _SHIM_NOTIFY_H
#define _SHIM_NOTIFY_H
#define NOTIFICATION_EVENT_TIME_CHANGE 1
#define NOTIFICATION_EVENT_TZ_CHANGE   12
BOOL CeEventHasOccurred(LONG lWhichEvent, LPWSTR pwszEndOfCommandLine);
#endif
