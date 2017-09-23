#ifndef MONSOON_HISTORY_EXPORT__H
#define MONSOON_HISTORY_EXPORT__H

/*
 * Various macros to control symbol visibility in libraries.
 */
#if defined(WIN32)
# ifdef monsoon_history_EXPORTS
#   define monsoon_history_export_     __declspec(dllexport)
#   define monsoon_history_local_      /* nothing */
# else
#   define monsoon_history_export_     __declspec(dllimport)
#   define monsoon_history_local_      /* nothing */
# endif
#elif defined(__GNUC__) || defined(__clang__)
# define monsoon_history_export_       __attribute__ ((visibility ("default")))
# define monsoon_history_local_        __attribute__ ((visibility ("hidden")))
#else
# define monsoon_history_export_       /* nothing */
# define monsoon_history_local_        /* nothing */
#endif

#endif /* MONSOON_HISTORY_EXPORT__H */
