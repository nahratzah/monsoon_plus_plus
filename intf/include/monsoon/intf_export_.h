#ifndef MONSOON_INTF_EXPORT__H
#define MONSOON_INTF_EXPORT__H

/*
 * Various macros to control symbol visibility in libraries.
 */
#if defined(WIN32)
# ifdef monsoon_intf_EXPORTS
#   define monsoon_intf_export_        __declspec(dllexport)
#   define monsoon_intf_local_         /* nothing */
# else
#   define monsoon_intf_export_        __declspec(dllimport)
#   define monsoon_intf_local_         /* nothing */
# endif
#elif defined(__GNUC__) || defined(__clang__)
# define monsoon_intf_export_          __attribute__ ((visibility ("default")))
# define monsoon_intf_local_           __attribute__ ((visibility ("hidden")))
#else
# define monsoon_intf_export_          /* nothing */
# define monsoon_intf_local_           /* nothing */
#endif

#endif /* MONSOON_INTF_EXPORT__H */
