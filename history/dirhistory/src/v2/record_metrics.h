#ifndef V2_RECORD_METRICS_H
#define V2_RECORD_METRICS_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <cstdint>
#include "../dynamics.h"
#include "fwd.h"
#include "cache.h"
#include "dictionary.h"
#include "encdec_ctx.h"
#include <vector>
#include <memory>
#include <utility>
#include <monsoon/path_matcher.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/xdr/xdr.h>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/filter_iterator.hpp>

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ record_metrics
: public typed_dynamics<record_array>,
  public std::enable_shared_from_this<record_metrics>
{
 private:
  using data_type = std::vector<std::pair<std::uint32_t, metric_value>, cache_allocator<std::pair<std::uint32_t, metric_value>>>;

 public:
  class monsoon_dirhistory_local_ proxy {
   public:
    proxy(std::shared_ptr<const dictionary> dict, data_type::const_pointer item)
    : dict_(std::move(dict)),
      item_(item)
    {}

    auto name() const
    -> metric_name {
      return dict_->pdd()[item_->first];
    }

    auto get() const
    -> const metric_value& {
      return item_->second;
    }

   private:
    std::shared_ptr<const dictionary> dict_;
    data_type::const_pointer item_;
  };

 private:
  class proxy_tf_fn_ {
   public:
    proxy_tf_fn_(std::shared_ptr<const dictionary> dict)
    : dict_(std::move(dict))
    {}

    auto operator()(data_type::const_reference v) const -> proxy;

   private:
    std::shared_ptr<const dictionary> dict_;
  };

  class filter_fn_ {
   public:
    filter_fn_(const path_matcher& m, std::shared_ptr<const dictionary> dict)
    : m_(&m),
      dict_(std::move(dict))
    {}

    auto operator()(data_type::const_reference v) const -> bool;

   private:
    const path_matcher* m_;
    std::shared_ptr<const dictionary> dict_;
  };

 public:
  using value_type = proxy;
  using allocator_type = typename std::allocator_traits<data_type::allocator_type>::rebind_alloc<value_type>;
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
    filter_view() noexcept {}

    filter_view(const record_metrics* self, const path_matcher* m) noexcept
    : self_(self),
      m_(m)
    {}

    auto begin() const -> const_filtered_iterator {
      return self_->begin(*m_);
    }

    auto end() const -> const_filtered_iterator {
      return self_->end(*m_);
    }

   private:
    const record_metrics* self_ = nullptr;
    const path_matcher* m_ = nullptr;
  };

  static constexpr bool is_compressed = true;

  explicit record_metrics(std::shared_ptr<record_array> parent, allocator_type alloc = allocator_type())
  : typed_dynamics<record_array>(std::move(parent)),
    data_(alloc)
  {}

  ~record_metrics() noexcept override;

  auto get_dictionary() const -> std::shared_ptr<const dictionary>;
  auto get_dictionary() -> std::shared_ptr<dictionary>;
  auto get_ctx() const -> encdec_ctx;

  auto decode(xdr::xdr_istream& in) -> void;

  auto begin() const -> const_iterator;
  auto end() const -> const_iterator;
  auto begin(const path_matcher& m) const -> const_filtered_iterator;
  auto end(const path_matcher& m) const -> const_filtered_iterator;

 private:
  data_type data_;
};


inline auto record_metrics::proxy_tf_fn_::operator()(data_type::const_reference v) const
-> proxy {
  return proxy(dict_, &v);
}

inline auto record_metrics::filter_fn_::operator()(data_type::const_reference v) const
-> bool {
  return (*m_)(metric_name(dict_->pdd()[v.first]));
}

inline auto record_metrics::begin() const
-> const_iterator {
  return boost::iterators::make_transform_iterator(
      data_.begin(),
      proxy_tf_fn_(get_dictionary()));
}

inline auto record_metrics::end() const
-> const_iterator {
  return boost::iterators::make_transform_iterator(
      data_.end(),
      proxy_tf_fn_(get_dictionary()));
}

inline auto record_metrics::begin(const path_matcher& m) const
-> const_filtered_iterator {
  std::shared_ptr<const dictionary> dict = get_dictionary();
  return boost::make_transform_iterator(
      boost::iterators::make_filter_iterator(
          filter_fn_(m, dict),
          data_.begin(), data_.end()),
      proxy_tf_fn_(dict));
}

inline auto record_metrics::end(const path_matcher& m) const
-> const_filtered_iterator {
  std::shared_ptr<const dictionary> dict = get_dictionary();
  return boost::make_transform_iterator(
      boost::iterators::make_filter_iterator(
          filter_fn_(m, dict),
          data_.end(), data_.end()),
      proxy_tf_fn_(dict));
}


} /* namespace monsoon::history::v2 */

#endif /* V2_RECORD_METRICS_H */
