#ifndef MONSOON_TX_DETAIL_EXPORT__H
#define MONSOON_TX_DETAIL_EXPORT__H

/*
 * Various macros to control symbol visibility in libraries.
 */
#if defined(WIN32)
# ifdef monsoon_tx_EXPORTS
#   define monsoon_tx_export_  __declspec(dllexport)
#   define monsoon_tx_local_   /* nothing */
# else
#   define monsoon_tx_export_  __declspec(dllimport)
#   define monsoon_tx_local_   /* nothing */
# endif
#elif defined(__GNUC__) || defined(__clang__)
# define monsoon_tx_export_    __attribute__ ((visibility ("default")))
# define monsoon_tx_local_     __attribute__ ((visibility ("hidden")))
#else
# define monsoon_tx_export_    /* nothing */
# define monsoon_tx_local_     /* nothing */
#endif

#endif /* MONSOON_TX_DETAIL_EXPORT__H */
