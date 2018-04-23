#include <instrumentation/group.h>
#include <monsoon/intf_export_.h>

///\file
///\ingroup intf

namespace monsoon {


/**
 * \brief All internal metrics of monsoon are grouped under this metric.
 * \ingroup intf
 */
monsoon_intf_export_
instrumentation::group& monsoon_instrumentation();

/**
 * \brief All internal caches used by monsoon.
 * \ingroup intf
 */
monsoon_intf_export_
instrumentation::group& cache_instrumentation();


} /* namespace monsoon */
