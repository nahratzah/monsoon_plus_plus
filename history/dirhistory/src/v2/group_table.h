#ifndef V2_GROUP_TABLE_H
#define V2_GROUP_TABLE_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/metric_name.h>
#include <monsoon/memoid.h>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <utility>
#include <boost/iterator/transform_iterator.hpp>
#include "fwd.h"
#include "bitset.h"
#include "encdec_ctx.h"
#include "file_segment_ptr.h"
#include "../dynamics.h"

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ group_table
: public typed_dynamics<void>,
  public std::enable_shared_from_this<group_table>
{
 private:
  using data_type = std::unordered_map<std::uint32_t, file_segment_ptr>;

 public:
  using size_type = bitset::size_type;

  class proxy;

 private:
  class proxy_tf_fn_ {
   public:
    explicit proxy_tf_fn_(std::shared_ptr<const group_table> owner)
    : owner_(std::move(owner))
    {}

    auto operator()(data_type::const_reference v) const -> proxy;

   private:
    std::shared_ptr<const group_table> owner_;
  };

 public:
  using const_iterator = boost::iterators::transform_iterator<
      proxy_tf_fn_,
      data_type::const_iterator>;
  using iterator = const_iterator;

  [[deprecated]]
  group_table(std::shared_ptr<dictionary> dict, encdec_ctx ctx)
  : group_table(nullptr, std::move(dict), std::move(ctx))
  {}

  group_table(std::shared_ptr<void> parent, std::shared_ptr<dictionary> dict, encdec_ctx ctx)
  : typed_dynamics<void>(std::move(parent)),
    dict_(std::move(dict)),
    ctx_(std::move(ctx))
  {}

  ~group_table() noexcept override;

  auto get_dictionary()
  -> std::shared_ptr<dictionary> {
    return dict_;
  }

  auto get_dictionary() const
  -> std::shared_ptr<const dictionary> {
    return dict_;
  }

  auto get_ctx() const
  -> const encdec_ctx& {
    return ctx_;
  }

  auto size() const
  noexcept
  -> size_type {
    return presence_.size();
  }

  auto presence() const
  noexcept
  -> const bitset& {
    return presence_;
  }

  auto begin() const -> const_iterator;
  auto end() const -> const_iterator;

  static auto from_xdr(
      std::shared_ptr<void> parent,
      xdr::xdr_istream& in,
      std::shared_ptr<dictionary> dict,
      encdec_ctx ctx)
      -> std::shared_ptr<group_table>;
  auto decode(xdr::xdr_istream& in) -> void;

 private:
  auto read_(data_type::const_reference item) const -> std::shared_ptr<const metric_table>;

  bitset presence_;
  data_type data_;
  std::shared_ptr<dictionary> dict_; // XXX: Shift to dynamics::parent lookup.
  encdec_ctx ctx_; // XXX: Shift to dynamics::parent lookup.
};


class monsoon_dirhistory_local_ group_table::proxy {
 public:
  proxy(std::shared_ptr<const group_table> owner, data_type::const_pointer item)
  : owner_(std::move(owner)),
    item_(item),
    mt_([this]() { return owner_->read_(*item_); })
  {}

  proxy(const proxy& y)
  : proxy(y.owner_, y.item_)
  {}

  proxy(proxy&& y)
  : proxy(std::move(y.owner_), y.item_)
  {
    y.item_ = nullptr;
    y.mt_.reset();
  }

  proxy& operator=(const proxy& y) {
    owner_ = y.owner_;
    item_ = y.item_;
    mt_.reset();
    return *this;
  }

  proxy& operator=(proxy&& y) {
    owner_ = std::move(y.owner_);
    item_ = std::move(y.item_);
    y.mt_.reset();
    mt_.reset();
    return *this;
  }

  auto name() const -> metric_name;

  auto operator*() const
  -> const metric_table& {
    return *get();
  }

  auto operator->() const
  -> std::shared_ptr<const metric_table> {
    return get();
  }

  auto get() const
  -> std::shared_ptr<const metric_table> {
    return mt_.get();
  }

 private:
  std::shared_ptr<const group_table> owner_;
  data_type::const_pointer item_;
  memoid<std::shared_ptr<const metric_table>> mt_;
};

inline auto group_table::proxy_tf_fn_::operator()(data_type::const_reference v) const
-> proxy {
  return proxy(owner_, &v);
}

inline auto group_table::begin() const
-> const_iterator {
  return const_iterator(
      data_.begin(),
      proxy_tf_fn_(shared_from_this()));
}

inline auto group_table::end() const
-> const_iterator {
  return const_iterator(
      data_.end(),
      proxy_tf_fn_(shared_from_this()));
}


} /* namespace monsoon::history::v2 */

#endif /* V2_GROUP_TABLE_H */
