#include "record_metrics.h"
#include "record_array.h"
#include "xdr_primitives.h"

namespace monsoon::history::v2 {


record_metrics::~record_metrics() noexcept {}

auto record_metrics::get_dictionary() const
-> std::shared_ptr<const dictionary> {
  return parent().get_dictionary();
}

auto record_metrics::get_dictionary()
-> std::shared_ptr<dictionary> {
  return parent().get_dictionary();
}

auto record_metrics::get_ctx() const
-> encdec_ctx {
  return parent().get_ctx();
}

auto record_metrics::decode(xdr::xdr_istream& in)
-> void {
  auto dict = get_dictionary();

  data_.clear();
  in.get_collection(
      [dict](xdr::xdr_istream& in) {
        std::uint32_t path_ref = in.get_uint32();
        metric_value mv = decode_metric_value(in, dict->sdd());
        return std::make_tuple(std::move(path_ref), std::move(mv));
      },
      data_);
}


} /* namespace monsoon::history::v2 */
