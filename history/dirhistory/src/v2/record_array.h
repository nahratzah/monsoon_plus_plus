#ifndef V2_RECORD_ARRAY_H
#define V2_RECORD_ARRAY_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <cstdint>
#include "fwd.h"
#include "dictionary.h"
#include "cache.h"
#include "file_segment_ptr.h"
#include "encdec_ctx.h"
#include "../dynamics.h"
#include <memory>
#include <utility>
#include <monsoon/xdr/xdr.h>
#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <monsoon/group_name.h>
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ record_array
: public typed_dynamics<tsdata_xdr>,
  public std::enable_shared_from_this<record_array>
{
 private:
  struct elem {
    std::uint32_t grp_ref;
    std::uint32_t tag_ref;
    file_segment_ptr metrics;
  };

  using data_type = std::vector<elem, cache_allocator<elem>>;

 public:
  class monsoon_dirhistory_local_ proxy {
   public:
    proxy(
        std::shared_ptr<const record_array> owner,
        std::shared_ptr<const dictionary> dict,
        data_type::const_pointer item)
    : owner_(std::move(owner)),
      dict_(std::move(dict)),
      item_(std::move(item)),
      rm_([this]() { return owner_->read_(*item_); })
    {}

    proxy(const proxy& y)
    : proxy(y.owner_, y.dict_, y.item_)
    {}

    proxy(proxy&& y)
    : proxy(std::move(y.owner_), std::move(y.dict_), y.item_)
    {
      y.item_ = nullptr;
      y.rm_.reset();
    }

    proxy& operator=(const proxy& y) {
      owner_ = y.owner_;
      dict_ = y.dict_;
      item_ = y.item_;
      rm_.reset();
      return *this;
    }

    proxy& operator=(proxy&& y) {
      owner_ = std::move(y.owner_);
      dict_ = std::move(y.dict_);
      item_ = std::move(y.item_);
      y.rm_.reset();
      rm_.reset();
      return *this;
    }

    auto path() const -> simple_group {
      return dict_->pdd()[item_->grp_ref];
    }

    auto tags() const -> monsoon::tags {
      return dict_->tdd()[item_->tag_ref];
    }

    auto name() const -> group_name {
      return group_name(path(), tags());
    }

    auto operator*() const
    -> const record_metrics& {
      return *get();
    }

    auto operator->() const
    -> std::shared_ptr<const record_metrics> {
      return get();
    }

    auto get() const
    -> std::shared_ptr<const record_metrics> {
      return rm_.get();
    }

   private:
    std::shared_ptr<const record_array> owner_;
    std::shared_ptr<const dictionary> dict_;
    data_type::const_pointer item_;
    memoid<std::shared_ptr<const record_metrics>> rm_;
  };

 private:
  class proxy_tf_fn_ {
   public:
    explicit proxy_tf_fn_(std::shared_ptr<const record_array> owner)
    : owner_(std::move(owner)),
      dict_(owner_->get_dictionary())
    {}

    proxy_tf_fn_(std::shared_ptr<const record_array> owner, std::shared_ptr<const dictionary> dict)
    : owner_(std::move(owner)),
      dict_(std::move(dict))
    {}

    auto operator()(data_type::const_reference v) const -> proxy;

   private:
    std::shared_ptr<const record_array> owner_;
    std::shared_ptr<const dictionary> dict_;
  };

  class filter_fn_ {
   public:
    explicit filter_fn_(const path_matcher& g, const tag_matcher& t, std::shared_ptr<const dictionary> dict) noexcept
    : g_(&g),
      t_(&t),
      dict_(std::move(dict))
    {}

    auto operator()(data_type::const_reference v) const -> bool;

   private:
    const path_matcher* g_;
    const tag_matcher* t_;
    std::shared_ptr<const dictionary> dict_;
  };

 public:
  using value_type = proxy;
  using allocator_type = std::allocator_traits<data_type::allocator_type>::rebind_alloc<value_type>;
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
    explicit filter_view(const record_array* self, const path_matcher* g, const tag_matcher* t)
    noexcept
    : self_(self),
      g_(g),
      t_(t)
    {}

    auto begin() const
    -> const_filtered_iterator {
      return self_->begin(*g_, *t_);
    }

    auto end() const
    -> const_filtered_iterator {
      return self_->end(*g_, *t_);
    }

   private:
    const record_array* self_ = nullptr;
    const path_matcher* g_ = nullptr;
    const tag_matcher* t_ = nullptr;
  };

  static constexpr bool is_compressed = true;

  explicit record_array(std::shared_ptr<tsdata_xdr> parent, allocator_type alloc = allocator_type())
  : typed_dynamics<tsdata_xdr>(std::move(parent)),
    data_(alloc)
  {}

  ~record_array() noexcept override;

  auto get_dictionary() const -> std::shared_ptr<const dictionary>;
  auto get_dictionary() -> std::shared_ptr<dictionary>;
  auto get_ctx() const -> encdec_ctx;

  auto begin() const -> const_iterator;
  auto end() const -> const_iterator;
  auto begin(const path_matcher& g, const tag_matcher& t) const -> const_filtered_iterator;
  auto end(const path_matcher& g, const tag_matcher& t) const -> const_filtered_iterator;
  auto filter(const path_matcher& g, const tag_matcher& t) const -> filter_view {
    return filter_view(this, &g, &t);
  }

  auto decode(xdr::xdr_istream& in) -> void;

 private:
  auto read_(const elem& v) const -> std::shared_ptr<record_metrics>;

  data_type data_;
};


inline auto record_array::proxy_tf_fn_::operator()(data_type::const_reference v) const
-> proxy {
  return proxy(owner_, dict_, &v);
}

inline auto record_array::filter_fn_::operator()(data_type::const_reference v) const
-> bool {
  return (*g_)(simple_group(dict_->pdd()[v.grp_ref]))
      && (*t_)(monsoon::tags(dict_->tdd()[v.tag_ref]));
}

inline auto record_array::begin() const
-> const_iterator {
  return boost::iterators::make_transform_iterator(
      data_.begin(),
      proxy_tf_fn_(shared_from_this()));
}

inline auto record_array::end() const
-> const_iterator {
  return boost::iterators::make_transform_iterator(
      data_.end(),
      proxy_tf_fn_(shared_from_this()));
}

inline auto record_array::begin(const path_matcher& g, const tag_matcher& t) const
-> const_filtered_iterator {
  std::shared_ptr<const dictionary> dict = get_dictionary();
  return boost::iterators::make_transform_iterator(
      boost::iterators::make_filter_iterator(
          filter_fn_(g, t, dict),
          data_.begin(),
          data_.end()),
      proxy_tf_fn_(shared_from_this(), dict));
}

inline auto record_array::end(const path_matcher& g, const tag_matcher& t) const
-> const_filtered_iterator {
  std::shared_ptr<const dictionary> dict = get_dictionary();
  return boost::iterators::make_transform_iterator(
      boost::iterators::make_filter_iterator(
          filter_fn_(g, t, dict),
          data_.end(),
          data_.end()),
      proxy_tf_fn_(shared_from_this(), dict));
}


} /* namespace monsoon::history::v2 */

#endif /* V2_RECORD_ARRAY_H */
