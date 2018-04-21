#ifndef V2_PRIMITIVES_H
#define V2_PRIMITIVES_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/metric_value.h>
#include <monsoon/histogram.h>
#include <monsoon/xdr/xdr.h>
#include "dictionary.h"

namespace monsoon::history::v2 {


monsoon_dirhistory_local_
auto decode_metric_value(
    monsoon::xdr::xdr_istream& in,
    const strval_dictionary& dict)
-> metric_value;

monsoon_dirhistory_local_
auto encode_metric_value(
    monsoon::xdr::xdr_ostream& in,
    const metric_value& v,
    strval_dictionary& dict)
-> void;

monsoon_dirhistory_local_
auto decode_histogram(monsoon::xdr::xdr_istream& in)
-> histogram;

monsoon_dirhistory_local_
auto encode_histogram(monsoon::xdr::xdr_ostream& in, const histogram& h)
-> void;


} /* namespace monsoon::history::v2 */

#endif /* V2_PRIMITIVES_H */
