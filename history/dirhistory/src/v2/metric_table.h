#ifndef V2_METRIC_TABLE_H
#define V2_METRIC_TABLE_H

#include <cassert>
#include <optional>
#include <vector>
#include <monsoon/metric_value.h>
#include <monsoon/xdr/xdr.h>
#include <monsoon/history/dir/dirhistory_export_.h>
#include "dictionary.h"
#include "../dynamics.h"

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ metric_table
: public dynamics,
  public std::enable_shared_from_this<metric_table>
{
 public:
  using value_type = std::optional<metric_value>;

 private:
  using data_type = std::vector<value_type>;

 public:
  using iterator = data_type::const_iterator;
  using const_iterator = iterator;
  using size_type = data_type::size_type;

  metric_table() = default;
  ~metric_table() noexcept;

  auto decode(xdr::xdr_istream& in, const std::shared_ptr<const strval_dictionary>& dict) -> void;

  auto size() const
  noexcept
  -> size_type {
    return data_.size();
  }

  auto reserve(size_type n)
  -> void {
    data_.reserve(n);
  }

  auto begin() const
  -> const_iterator {
    return data_.begin();
  }

  auto end() const
  -> const_iterator {
    return data_.end();
  }

  auto cbegin() const
  -> const_iterator {
    return begin();
  }

  auto cend() const
  -> const_iterator {
    return end();
  }

  auto operator[](size_type idx) const
  -> const value_type& {
    assert(idx >= 0 && idx < data_.size());
    return data_[idx];
  }

  auto present(size_type idx) const
  -> bool {
    assert(idx >= 0 && idx < data_.size());
    return data_[idx].has_value();
  }

 private:
  data_type data_;
};


} /* namespace monsoon::history::v2 */

#endif /* V2_METRIC_TABLE_H */
