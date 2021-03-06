#ifndef MONSOON_EXPR_EXPORT__H
#define MONSOON_EXPR_EXPORT__H

/*
 * Various macros to control symbol visibility in libraries.
 */
#if defined(WIN32)
# ifdef monsoon_expr_EXPORTS
#   define monsoon_expr_export_        __declspec(dllexport)
#   define monsoon_expr_local_         /* nothing */
# else
#   define monsoon_expr_export_        __declspec(dllimport)
#   define monsoon_expr_local_         /* nothing */
# endif
#elif defined(__GNUC__) || defined(__clang__)
# define monsoon_expr_export_          __attribute__ ((visibility ("default")))
# define monsoon_expr_local_           __attribute__ ((visibility ("hidden")))
#else
# define monsoon_expr_export_          /* nothing */
# define monsoon_expr_local_           /* nothing */
#endif

#endif /* MONSOON_EXPR_EXPORT__H */
