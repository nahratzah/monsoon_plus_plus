#include "group_table.h"
#include "metric_table.h"
#include "dictionary.h"
#include <monsoon/history/dir/hdir_exception.h>

namespace monsoon::history::v2 {


group_table::~group_table() noexcept {}

auto group_table::from_xdr(
    std::shared_ptr<void> parent,
    xdr::xdr_istream& in,
    std::shared_ptr<dictionary> dict,
    encdec_ctx ctx)
-> std::shared_ptr<group_table> {
  auto tbl = std::make_shared<group_table>(parent, dict, ctx);
  tbl->decode(in);
  return tbl;
}

auto group_table::decode(xdr::xdr_istream& in)
-> void {
  presence_.decode(in);

  data_.clear();
  in.get_collection(
      [](xdr::xdr_istream& in) {
        std::uint32_t metric_name_ref = in.get_uint32();
        file_segment_ptr ptr = file_segment_ptr::from_xdr(in);
        return std::make_pair(metric_name_ref, ptr);
      },
      data_);
}

auto group_table::read_(data_type::const_reference ptr) const
-> std::shared_ptr<const metric_table> {
  auto xdr = get_ctx().new_reader(ptr.second, metric_table::is_compressed);
  auto result = metric_table::from_xdr(std::const_pointer_cast<group_table>(shared_from_this()), xdr);
  if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
  xdr.close();
  return result;
}


auto group_table::proxy::name() const
-> metric_name {
  return owner_->get_dictionary()->pdd()[item_->first];
}


} /* namespace monsoon::history::v2 */
