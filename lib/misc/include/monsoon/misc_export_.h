#ifndef MONSOON_MISC_EXPORT__H
#define MONSOON_MISC_EXPORT__H

/*
 * Various macros to control symbol visibility in libraries.
 */
#if defined(WIN32)
# ifdef monsoon_misc_EXPORTS
#   define monsoon_misc_export_        __declspec(dllexport)
#   define monsoon_misc_local_         /* nothing */
# else
#   define monsoon_misc_export_        __declspec(dllimport)
#   define monsoon_misc_local_         /* nothing */
# endif
#elif defined(__GNUC__) || defined(__clang__)
# define monsoon_misc_export_          __attribute__ ((visibility ("default")))
# define monsoon_misc_local_           __attribute__ ((visibility ("hidden")))
#else
# define monsoon_misc_export_          /* nothing */
# define monsoon_misc_local_           /* nothing */
#endif

#endif /* MONSOON_MISC_EXPORT__H */
