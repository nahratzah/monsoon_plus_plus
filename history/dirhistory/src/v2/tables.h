#ifndef V2_TABLES_H
#define V2_TABLES_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <monsoon/group_name.h>
#include <monsoon/memoid.h>
#include "fwd.h"
#include "file_segment_ptr.h"
#include "encdec_ctx.h"
#include "cache.h"
#include "../dynamics.h"
#include <cstdint>
#include <utility>
#include <unordered_map>
#include <memory>
#include <boost/iterator/transform_iterator.hpp>

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ tables
: public typed_dynamics<file_data_tables_block>,
  public std::enable_shared_from_this<tables>
{
 private:
  struct key_type {
    std::uint32_t grp_ref, tag_ref;

    auto operator==(const key_type& y) const
    noexcept
    -> bool {
      return grp_ref == y.grp_ref && tag_ref == y.tag_ref;
    }

    auto operator!=(const key_type& y) const
    noexcept
    -> bool {
      return !(*this == y);
    }
  };

  struct key_hash {
    auto operator()(const key_type& k) const
    noexcept
    -> std::size_t {
      std::hash<std::uint32_t> int_hash;
      return (73u * int_hash(k.grp_ref)) ^ (257u * int_hash(k.tag_ref));
    }
  };

  using data_type = std::unordered_map<key_type, file_segment_ptr, key_hash, std::equal_to<key_type>, cache_allocator<std::pair<const key_type, file_segment_ptr>>>;

 public:
  class proxy;

 private:
  class proxy_tf_fn_ {
   public:
    explicit proxy_tf_fn_(std::shared_ptr<const tables> owner)
    : owner_(std::move(owner))
    {}

    auto operator()(data_type::const_reference v) const -> proxy;

   private:
    std::shared_ptr<const tables> owner_;
  };

 public:
  using allocator_type = data_type::allocator_type;
  using size_type = data_type::size_type;
  using const_iterator = boost::iterators::transform_iterator<
      proxy_tf_fn_,
      data_type::const_iterator>;
  using iterator = const_iterator;

  static constexpr bool is_compressed = true;

  tables(std::shared_ptr<file_data_tables_block> parent, allocator_type alloc = allocator_type())
  : typed_dynamics<file_data_tables_block>(std::move(parent)),
    data_(alloc)
  {}

  ~tables() noexcept override;

  auto get_dictionary() -> std::shared_ptr<dictionary>;
  auto get_dictionary() const -> std::shared_ptr<const dictionary>;
  auto get_ctx() const -> const encdec_ctx&;

  auto size() const
  noexcept
  -> size_type {
    return data_.size();
  }

  auto begin() const -> const_iterator;
  auto end() const -> const_iterator;

  static auto from_xdr(
      std::shared_ptr<file_data_tables_block> parent,
      xdr::xdr_istream& in)
      -> std::shared_ptr<tables>;
  auto decode(xdr::xdr_istream& in) -> void;

 private:
  auto read_(data_type::const_reference item) const -> std::shared_ptr<const group_table>;

  data_type data_;
};


class monsoon_dirhistory_local_ tables::proxy {
 public:
  proxy(std::shared_ptr<const tables> owner, data_type::const_pointer item)
  : owner_(std::move(owner)),
    item_(item),
    gt_([this]() { return owner_->read_(*item_); })
  {}

  proxy(const proxy& y)
  : proxy(y.owner_, y.item_)
  {}

  proxy(proxy&& y)
  : proxy(std::move(y.owner_), y.item_)
  {
    y.item_ = nullptr;
    y.gt_.reset();
  }

  proxy& operator=(const proxy& y) {
    owner_ = y.owner_;
    item_ = y.item_;
    gt_.reset();
    return *this;
  }

  proxy& operator=(proxy&& y) {
    owner_ = std::move(y.owner_);
    item_ = std::move(y.item_);
    y.gt_.reset();
    gt_.reset();
    return *this;
  }

  auto path() const -> simple_group;
  auto tags() const -> monsoon::tags;
  auto name() const -> group_name;

  auto operator*() const
  -> const group_table& {
    return *get();
  }

  auto operator->() const
  -> std::shared_ptr<const group_table> {
    return get();
  }

  auto get() const
  -> std::shared_ptr<const group_table> {
    return gt_.get();
  }

 private:
  std::shared_ptr<const tables> owner_;
  data_type::const_pointer item_;
  memoid<std::shared_ptr<const group_table>> gt_;
};

inline auto tables::proxy_tf_fn_::operator()(data_type::const_reference v) const
-> proxy {
  return proxy(owner_, &v);
}

inline auto tables::begin() const
-> const_iterator {
  return const_iterator(
      data_.begin(),
      proxy_tf_fn_(shared_from_this()));
}

inline auto tables::end() const
-> const_iterator {
  return const_iterator(
      data_.end(),
      proxy_tf_fn_(shared_from_this()));
}


} /* namespace monsoon::history::v2 */

#endif /* V2_TABLES_H */
