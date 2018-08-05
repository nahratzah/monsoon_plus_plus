#ifndef V2_TABLES_H
#define V2_TABLES_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <monsoon/group_name.h>
#include <monsoon/path_matcher.h>
#include <monsoon/tag_matcher.h>
#include <monsoon/memoid.h>
#include "fwd.h"
#include "file_segment_ptr.h"
#include "encdec_ctx.h"
#include "cache.h"
#include "dictionary.h"
#include "../dynamics.h"
#include <cstdint>
#include <utility>
#include <vector>
#include <memory>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>

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

  using data_type = std::vector<std::pair<key_type, file_segment_ptr>, cache_allocator<std::pair<key_type, file_segment_ptr>>>;

 public:
  class monsoon_dirhistory_local_ proxy {
   public:
    proxy(std::shared_ptr<const tables> owner, std::shared_ptr<const dictionary> dict, data_type::const_pointer item)
    : owner_(std::move(owner)),
      dict_(std::move(dict)),
      item_(item),
      gt_([this]() { return owner_->read_(*item_); })
    {}

    proxy(const proxy& y)
    : proxy(y.owner_, y.dict_, y.item_)
    {}

    proxy(proxy&& y)
    : proxy(std::move(y.owner_), std::move(y.dict_), y.item_)
    {
      y.item_ = nullptr;
      y.gt_.reset();
    }

    proxy& operator=(const proxy& y) {
      owner_ = y.owner_;
      dict_ = y.dict_;
      item_ = y.item_;
      gt_.reset();
      return *this;
    }

    proxy& operator=(proxy&& y) {
      owner_ = std::move(y.owner_);
      dict_ = std::move(y.dict_);
      item_ = std::move(y.item_);
      y.gt_.reset();
      gt_.reset();
      return *this;
    }

    auto path() const -> simple_group {
      return dict_->pdd()[item_->first.grp_ref];
    }

    auto tags() const -> monsoon::tags {
      return dict_->tdd()[item_->first.tag_ref];
    }

    auto name() const -> group_name {
      return group_name(path(), tags());
    }

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
    std::shared_ptr<const dictionary> dict_;
    data_type::const_pointer item_;
    memoid<std::shared_ptr<const group_table>> gt_;
  };

 private:
  class proxy_tf_fn_ {
   public:
    explicit proxy_tf_fn_(std::shared_ptr<const tables> owner)
    : owner_(std::move(owner)),
      dict_(owner_->get_dictionary())
    {}

    explicit proxy_tf_fn_(std::shared_ptr<const tables> owner, std::shared_ptr<const dictionary> dict)
    : owner_(std::move(owner)),
      dict_(std::move(dict))
    {}

    auto operator()(data_type::const_reference v) const -> proxy;

   private:
    std::shared_ptr<const tables> owner_;
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
  using allocator_type = data_type::allocator_type;
  using size_type = data_type::size_type;
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
    explicit filter_view(const tables* self, const path_matcher* g, const tag_matcher* t)
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
    const tables* self_ = nullptr;
    const path_matcher* g_ = nullptr;
    const tag_matcher* t_ = nullptr;
  };

  static constexpr bool is_compressed = true;

  tables(std::shared_ptr<file_data_tables_block> parent, allocator_type alloc = allocator_type())
  : typed_dynamics<file_data_tables_block>(std::move(parent)),
    data_(alloc)
  {}

  ~tables() noexcept override;

  auto get_dictionary() -> std::shared_ptr<dictionary>;
  auto get_dictionary() const -> std::shared_ptr<const dictionary>;
  auto get_ctx() const -> encdec_ctx;

  auto size() const
  noexcept
  -> size_type {
    return data_.size();
  }

  auto begin() const -> const_iterator;
  auto end() const -> const_iterator;
  auto begin(const path_matcher& g, const tag_matcher& t) const -> const_filtered_iterator;
  auto end(const path_matcher& g, const tag_matcher& t) const -> const_filtered_iterator;
  auto filter(const path_matcher& g, const tag_matcher& t) const -> filter_view {
    return filter_view(this, &g, &t);
  }

  static auto from_xdr(
      std::shared_ptr<file_data_tables_block> parent,
      xdr::xdr_istream& in)
      -> std::shared_ptr<tables>;
  auto decode(xdr::xdr_istream& in) -> void;

 private:
  auto read_(data_type::const_reference item) const -> std::shared_ptr<const group_table>;

  data_type data_;
};


inline auto tables::proxy_tf_fn_::operator()(data_type::const_reference v) const
-> proxy {
  return proxy(owner_, dict_, &v);
}

inline auto tables::filter_fn_::operator()(data_type::const_reference v) const
-> bool {
  return (*g_)(simple_group(dict_->pdd()[v.first.grp_ref]))
      && (*t_)(monsoon::tags(dict_->tdd()[v.first.tag_ref]));
}

inline auto tables::begin() const
-> const_iterator {
  return boost::iterators::make_transform_iterator(
      data_.begin(),
      proxy_tf_fn_(shared_from_this()));
}

inline auto tables::end() const
-> const_iterator {
  return boost::iterators::make_transform_iterator(
      data_.end(),
      proxy_tf_fn_(shared_from_this()));
}

inline auto tables::begin(const path_matcher& g, const tag_matcher& t) const
-> const_filtered_iterator {
  std::shared_ptr<const dictionary> dict = get_dictionary();
  return boost::iterators::make_transform_iterator(
      boost::iterators::make_filter_iterator(
          filter_fn_(g, t, dict),
          data_.begin(),
          data_.end()),
      proxy_tf_fn_(shared_from_this(), dict));
}

inline auto tables::end(const path_matcher& g, const tag_matcher& t) const
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

#endif /* V2_TABLES_H */
