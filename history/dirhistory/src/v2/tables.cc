#include "tables.h"
#include "group_table.h"
#include "dictionary.h"
#include "file_data_tables_block.h"
#include "cache.h"
#include <monsoon/history/dir/hdir_exception.h>

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
-> const encdec_ctx& {
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
#if __cplusplus >= 201703
          data_.try_emplace(key, std::get<1>(t));
#else
          data_.emplace(key, std::get<1>(t));
#endif
        }
      });
}

auto tables::read_(data_type::const_reference ptr) const -> std::shared_ptr<const group_table> {
  return get_dynamics_cache<group_table>(std::const_pointer_cast<tables>(shared_from_this()), ptr.second);
}


auto tables::proxy::path() const
-> simple_group {
  return owner_->get_dictionary()->pdd()[item_->first.grp_ref];
}

auto tables::proxy::tags() const
-> monsoon::tags {
  return owner_->get_dictionary()->tdd()[item_->first.tag_ref];
}

auto tables::proxy::name() const
-> group_name {
  auto dict = owner_->get_dictionary();
  return group_name(
      dict->pdd()[item_->first.grp_ref],
      dict->tdd()[item_->first.tag_ref]);
}


} /* namespace monsoon::history::v2 */
