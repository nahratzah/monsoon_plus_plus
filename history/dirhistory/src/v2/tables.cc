#include "tables.h"
#include "group_table.h"
#include "dictionary.h"
#include "file_data_tables_block.h"
#include "cache.h"
#include <monsoon/history/dir/hdir_exception.h>
#include <algorithm>

namespace monsoon::history::v2 {


tables::~tables() noexcept {}

auto tables::get_dictionary()
-> std::shared_ptr<dictionary> {
  return parent().get_dictionary();
}

auto tables::get_dictionary() const
-> std::shared_ptr<const dictionary> {
  return parent().get_dictionary();
}

auto tables::get_ctx() const
-> encdec_ctx {
  return parent().get_ctx();
}

auto tables::from_xdr(
    std::shared_ptr<file_data_tables_block> parent,
    xdr::xdr_istream& in)
-> std::shared_ptr<tables> {
  auto tbl = std::make_shared<tables>(std::move(parent));
  tbl->decode(in);
  return tbl;
}

auto tables::decode(xdr::xdr_istream& in) -> void {
  data_.clear();
  in.accept_collection(
      [](xdr::xdr_istream& in) {
        std::uint32_t grp_ref = in.get_uint32();
        auto tagmap = in.get_collection<std::vector<std::tuple<std::uint32_t, file_segment_ptr>>>(
            [](xdr::xdr_istream& in) {
              std::uint32_t tag_ref = in.get_uint32();
              file_segment_ptr ptr = file_segment_ptr::from_xdr(in);
              return std::make_tuple(std::move(tag_ref), std::move(ptr));
            });
        return std::make_tuple(std::move(grp_ref), std::move(tagmap));
      },
      [this](auto&& v) {
        key_type key;
        key.grp_ref = std::get<0>(v);
        for (const auto& t : std::get<1>(v)) {
          key.tag_ref = std::get<0>(t);
          data_.emplace_back(key, std::get<1>(t));
        }
      });

  std::sort(data_.begin(), data_.end(),
      [](const auto& x, const auto& y) noexcept {
        return (std::uint64_t(x.first.grp_ref) << 32 | x.first.tag_ref)
            < (std::uint64_t(y.first.grp_ref) << 32 | y.first.tag_ref);
      });
  data_.erase(
      std::unique(data_.begin(), data_.end(),
          [](const auto& x, const auto& y) noexcept {
            return (std::uint64_t(x.first.grp_ref) << 32 | x.first.tag_ref)
                == (std::uint64_t(y.first.grp_ref) << 32 | y.first.tag_ref);
          }),
      data_.end());
}

auto tables::read_(data_type::const_reference ptr) const -> std::shared_ptr<const group_table> {
  return get_dynamics_cache<group_table>(std::const_pointer_cast<tables>(shared_from_this()), ptr.second);
}


} /* namespace monsoon::history::v2 */
