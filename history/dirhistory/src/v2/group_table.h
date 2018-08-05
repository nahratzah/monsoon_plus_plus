#ifndef V2_GROUP_TABLE_H
#define V2_GROUP_TABLE_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/metric_name.h>
#include <monsoon/memoid.h>
#include <monsoon/path_matcher.h>
#include <vector>
#include <cstdint>
#include <memory>
#include <utility>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include "fwd.h"
#include "bitset.h"
#include "encdec_ctx.h"
#include "file_segment_ptr.h"
#include "cache.h"
#include "dictionary.h"
#include "../dynamics.h"

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ group_table
: public typed_dynamics<tables>,
  public std::enable_shared_from_this<group_table>
{
 private:
  using data_type = std::vector<std::pair<std::uint32_t, file_segment_ptr>, cache_allocator<std::pair<std::uint32_t, file_segment_ptr>>>;

 public:
  using allocator_type = data_type::allocator_type;
  using size_type = bitset::size_type;

  class monsoon_dirhistory_local_ proxy {
   public:
    proxy(std::shared_ptr<const group_table> owner, std::shared_ptr<const dictionary> dict, data_type::const_pointer item)
    : owner_(std::move(owner)),
      dict_(std::move(dict)),
      item_(item),
      mt_([this]() { return owner_->read_(*item_); })
    {}

    proxy(const proxy& y)
    : proxy(y.owner_, y.dict_, y.item_)
    {}

    proxy(proxy&& y)
    : proxy(std::move(y.owner_), std::move(y.dict_), y.item_)
    {
      y.item_ = nullptr;
      y.mt_.reset();
    }

    proxy& operator=(const proxy& y) {
      owner_ = y.owner_;
      dict_ = y.dict_;
      item_ = y.item_;
      mt_.reset();
      return *this;
    }

    proxy& operator=(proxy&& y) {
      owner_ = std::move(y.owner_);
      dict_ = std::move(y.dict_);
      item_ = std::move(y.item_);
      y.mt_.reset();
      mt_.reset();
      return *this;
    }

    auto name() const -> metric_name {
      return dict_->pdd()[item_->first];
    }

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
    std::shared_ptr<const dictionary> dict_;
    data_type::const_pointer item_;
    memoid<std::shared_ptr<const metric_table>> mt_;
  };

 private:
  class proxy_tf_fn_ {
   public:
    explicit proxy_tf_fn_(std::shared_ptr<const group_table> owner)
    : owner_(std::move(owner)),
      dict_(owner_->get_dictionary())
    {}

    explicit proxy_tf_fn_(std::shared_ptr<const group_table> owner, std::shared_ptr<const dictionary> dict)
    : owner_(std::move(owner)),
      dict_(std::move(dict))
    {}

    auto operator()(data_type::const_reference v) const -> proxy;

   private:
    std::shared_ptr<const group_table> owner_;
    std::shared_ptr<const dictionary> dict_;
  };

  class filter_fn_ {
   public:
    filter_fn_(const path_matcher& m, std::shared_ptr<const dictionary> dict) noexcept
    : m_(&m),
      dict_(std::move(dict))
    {}

    auto operator()(data_type::const_reference v) const -> bool;

   private:
    const path_matcher* m_;
    std::shared_ptr<const dictionary> dict_;
  };

 public:
  using const_iterator = boost::iterators::transform_iterator<
      proxy_tf_fn_,
      data_type::const_iterator>;
  using iterator = const_iterator;

  using const_filtered_iterator = boost::iterators::transform_iterator<
      proxy_tf_fn_,
      boost::iterators::filter_iterator<
          filter_fn_,
          data_type::const_iterator>>;
  using filtered_iterator = const_filtered_iterator;

  class filter_view {
   public:
    filter_view() noexcept
    : filter_view(nullptr, nullptr)
    {}

    explicit filter_view(const group_table* self, const path_matcher* m)
    noexcept
    : self_(self),
      m_(m)
    {}

    auto begin() const
    -> const_filtered_iterator {
      return self_->begin(*m_);
    }

    auto end() const
    -> const_filtered_iterator {
      return self_->end(*m_);
    }

   private:
    const group_table* self_ = nullptr;
    const path_matcher* m_ = nullptr;
  };

  static constexpr bool is_compressed = true;

  group_table(std::shared_ptr<tables> parent, allocator_type alloc = allocator_type())
  : typed_dynamics<tables>(std::move(parent)),
    presence_(alloc),
    data_(alloc)
  {}

  ~group_table() noexcept override;

  auto get_dictionary() -> std::shared_ptr<dictionary>;
  auto get_dictionary() const -> std::shared_ptr<const dictionary>;
  auto get_ctx() const -> encdec_ctx;

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
  auto begin(const path_matcher& m) const -> const_filtered_iterator;
  auto end(const path_matcher& m) const -> const_filtered_iterator;
  auto filter(const path_matcher& m) const -> filter_view {
    return filter_view(this, &m);
  }

  static auto from_xdr(std::shared_ptr<tables> parent, xdr::xdr_istream& in) -> std::shared_ptr<group_table>;
  auto decode(xdr::xdr_istream& in) -> void;

 private:
  auto read_(data_type::const_reference item) const -> std::shared_ptr<const metric_table>;

  bitset presence_;
  data_type data_;
};


inline auto group_table::proxy_tf_fn_::operator()(data_type::const_reference v) const
-> proxy {
  return proxy(owner_, dict_, &v);
}

inline auto group_table::filter_fn_::operator()(data_type::const_reference v) const -> bool {
  return (*m_)(metric_name(dict_->pdd()[v.first]));
}

inline auto group_table::begin() const
-> const_iterator {
  return boost::iterators::make_transform_iterator(
      data_.begin(),
      proxy_tf_fn_(shared_from_this()));
}

inline auto group_table::end() const
-> const_iterator {
  return boost::iterators::make_transform_iterator(
      data_.end(),
      proxy_tf_fn_(shared_from_this()));
}

inline auto group_table::begin(const path_matcher& m) const
-> const_filtered_iterator {
  std::shared_ptr<const dictionary> dict = get_dictionary();
  return boost::iterators::make_transform_iterator(
      boost::iterators::make_filter_iterator(
          filter_fn_(m, dict),
          data_.begin(),
          data_.end()),
      proxy_tf_fn_(shared_from_this(), dict));
}

inline auto group_table::end(const path_matcher& m) const
-> const_filtered_iterator {
  std::shared_ptr<const dictionary> dict = get_dictionary();
  return boost::iterators::make_transform_iterator(
      boost::iterators::make_filter_iterator(
          filter_fn_(m, dict),
          data_.end(),
          data_.end()),
      proxy_tf_fn_(shared_from_this(), dict));
}


} /* namespace monsoon::history::v2 */

#endif /* V2_GROUP_TABLE_H */
