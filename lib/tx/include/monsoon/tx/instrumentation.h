#ifndef MONSOON_TX_INSTRUMENTATION_H
#define MONSOON_TX_INSTRUMENTATION_H

#include <monsoon/tx/detail/export_.h>
#include <instrumentation/group.h>

namespace monsoon::tx {


monsoon_tx_export_
instrumentation::group& instrumentation();


}

namespace monsoon::tx::detail {


monsoon_tx_local_
instrumentation::group& monsoon_tx_instrumentation();


}

#endif /* MONSOON_TX_INSTRUMENTATION_H */
