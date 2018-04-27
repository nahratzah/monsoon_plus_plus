#include "record_array.h"
#include "record_metrics.h"
#include "tsdata_xdr.h"
#include <tuple>
#include <algorithm>

namespace monsoon::history::v2 {


record_array::~record_array() noexcept {}

auto record_array::get_dictionary() const -> std::shared_ptr<const dictionary> {
  return parent().get_dictionary();
}

auto record_array::get_dictionary() -> std::shared_ptr<dictionary> {
  return parent().get_dictionary();
}

auto record_array::get_ctx() const
-> encdec_ctx {
  return parent().get_ctx();
}

auto record_array::decode(xdr::xdr_istream& in)
-> void {
  data_.clear();
  in.accept_collection(
      [](xdr::xdr_istream& in) {
        elem e;
        e.grp_ref = in.get_uint32();
        return in.get_collection<std::vector<elem>>(
            [&e](xdr::xdr_istream& in) {
              e.tag_ref = in.get_uint32();
              e.metrics.decode(in);
              return e;
            });
      },
      [this](std::vector<elem>&& elems) {
        data_.insert(
            data_.end(),
            std::make_move_iterator(elems.begin()),
            std::make_move_iterator(elems.end()));
      });

  std::sort(data_.begin(), data_.end(),
      [](const elem& x, const elem& y) {
        return std::tie(x.grp_ref, x.tag_ref)
            < std::tie(y.grp_ref, y.tag_ref);
      });
  data_.erase(
      std::unique(data_.begin(), data_.end(),
          [](const elem& x, const elem& y) {
            return std::tie(x.grp_ref, x.tag_ref)
                == std::tie(y.grp_ref, y.tag_ref);
          }),
      data_.end());
}

auto record_array::read_(const elem& v) const
-> std::shared_ptr<record_metrics> {
  return get_dynamics_cache<record_metrics>(std::const_pointer_cast<record_array>(shared_from_this()), v.metrics);
}


} /* namespace monsoon::history::v2 */
