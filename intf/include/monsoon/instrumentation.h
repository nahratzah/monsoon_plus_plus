#include <instrumentation/engine.h>
#include <monsoon/intf_export_.h>
#include <monsoon/collector.h>

///\file
///\ingroup intf

namespace monsoon {


/**
 * \brief Instrumentation engine used internally by monsoon for self reporting.
 * \ingroup intf
 */
monsoon_intf_export_
instrumentation::engine monsoon_instrumentation();

/**
 * \brief Execute a collection of the internal metrics.
 */
monsoon_intf_export_
collector::collection monsoon_instrumentation_collect();


} /* namespace monsoon */
