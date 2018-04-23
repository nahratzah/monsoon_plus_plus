#ifndef V2_FILE_DATA_TABLES_H
#define V2_FILE_DATA_TABLES_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "../dynamics.h"
#include "fwd.h"
#include "cache.h"
#include "file_data_tables_block.h"
#include <memory>
#include <vector>
#include <utility>
#include <monsoon/memoid.h>
#include <monsoon/xdr/xdr.h>
#include <boost/iterator/transform_iterator.hpp>

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ file_data_tables
: public typed_dynamics<void>,
  public std::enable_shared_from_this<file_data_tables>
{
 private:
  using data_type = std::vector<file_data_tables_block, cache_allocator<file_data_tables_block>>;

  class transform_fn_ {
   public:
    explicit transform_fn_(std::shared_ptr<const file_data_tables> owner)
    : owner_(std::move(owner))
    {}

    auto operator()(data_type::const_reference v) const
    -> std::shared_ptr<const file_data_tables_block> {
      return std::shared_ptr<const file_data_tables_block>(owner_, &v);
    }

   private:
    std::shared_ptr<const file_data_tables> owner_;
  };

 public:
  using const_iterator = boost::iterators::transform_iterator<
      transform_fn_,
      data_type::const_iterator>;
  using iterator = const_iterator;
  using allocator_type = data_type::allocator_type;

  file_data_tables(std::shared_ptr<void> parent, encdec_ctx ctx, allocator_type alloc = allocator_type())
  : typed_dynamics<void>(std::move(parent)),
    blocks_(alloc),
    ctx_(ctx)
  {}

  ~file_data_tables() noexcept override;

  auto get_ctx() const
  -> const encdec_ctx& {
    return ctx_;
  }

  static auto from_xdr(std::shared_ptr<void> parent, xdr::xdr_istream& in, encdec_ctx ctx, allocator_type alloc = allocator_type())
  -> std::shared_ptr<file_data_tables> {
    auto tbl = std::allocate_shared<file_data_tables>(alloc, std::move(parent), std::move(ctx), alloc);
    tbl->decode(in);
    return tbl;
  }

  auto decode(xdr::xdr_istream& in) -> void;
  auto encode(xdr::xdr_ostream& out) const -> void;

  auto begin() const
  -> const_iterator {
    return boost::iterators::make_transform_iterator(
        blocks_.begin(),
        transform_fn_(shared_from_this()));
  }

  auto end() const
  -> const_iterator {
    return boost::iterators::make_transform_iterator(
        blocks_.end(),
        transform_fn_(shared_from_this()));
  }

 private:
  data_type blocks_;
  encdec_ctx ctx_;
};


} /* namespace monsoon::history::v2 */

#endif /* V2_FILE_DATA_TABLES_H */
