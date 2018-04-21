#ifndef V2_DICTIONARY_H
#define V2_DICTIONARY_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <monsoon/simple_group.h>
#include <monsoon/memoid.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <monsoon/xdr/xdr.h>
#include "../dynamics.h"

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ strval_dictionary {
 private:
  using inverse_map = std::unordered_map<std::string_view, std::uint32_t>;

 public:
  strval_dictionary();
  strval_dictionary(const strval_dictionary& y);
  strval_dictionary(strval_dictionary&& y);
  strval_dictionary& operator=(const strval_dictionary& y);
  strval_dictionary& operator=(strval_dictionary&& y);

  auto update_pending() const
  noexcept
  -> bool {
    return update_start_ < values_.size();
  }

  auto operator[](std::uint32_t idx) const -> std::string_view;
  auto operator[](std::string_view s) const -> std::uint32_t;
  auto operator[](std::string_view s) -> std::uint32_t;

  auto encode_update(xdr::xdr_ostream& out) -> void;
  auto decode_update(xdr::xdr_istream& in) -> void;

 private:
  auto make_inverse_() const -> inverse_map;

  std::vector<std::string> values_;
  memoid<inverse_map> inverse_;
  std::uint32_t update_start_ = 0;
};

class monsoon_dirhistory_local_ path_dictionary {
 private:
  using path = std::vector<std::uint32_t>;

  struct hasher_ {
    auto operator()(const path& p) const noexcept -> std::size_t {
      std::size_t h = psize_hash_fn_(p.size());
      for (std::uint32_t e : p)
        h = 53 * h + pelem_hash_fn_(e);
      return h;
    }

   private:
    std::hash<path::size_type> psize_hash_fn_;
    std::hash<std::uint32_t> pelem_hash_fn_;
  };

  using inverse_map = std::unordered_map<path, std::uint32_t, hasher_>;

 public:
  class proxy {
   private:
    auto make_transform_fn_() const {
      return [this](std::uint32_t str_ref) {
        return self_.str_tbl_[str_ref];
      };
    }

   public:
    proxy(const path_dictionary& self, std::uint32_t ref) noexcept
    : self_(self),
      ref_(ref)
    {}

    operator metric_name() const;
    operator simple_group() const;

   private:
    const path_dictionary& self_;
    std::uint32_t ref_;
  };

  path_dictionary(strval_dictionary& str_tbl);
  path_dictionary(const path_dictionary&) = delete;
  path_dictionary(const path_dictionary& y, strval_dictionary& str_tbl);
  path_dictionary(path_dictionary&& y, strval_dictionary& str_tbl);
  path_dictionary& operator=(const path_dictionary& y);
  path_dictionary& operator=(path_dictionary&& y);

  auto update_pending() const
  noexcept
  -> bool {
    return update_start_ < values_.size();
  }

  auto operator[](std::uint32_t idx) const -> proxy;
  auto operator[](const path_common& pc) const -> std::uint32_t;
  auto operator[](const path_common& pc) -> std::uint32_t;

  auto encode_update(xdr::xdr_ostream& out) -> void;
  auto decode_update(xdr::xdr_istream& in) -> void;

 private:
  auto make_inverse_() const -> inverse_map;

  strval_dictionary& str_tbl_;
  std::vector<path> values_;
  memoid<inverse_map> inverse_;
  std::uint32_t update_start_ = 0;
};

class monsoon_dirhistory_local_ tag_dictionary {
 private:
  using tag_data = std::unordered_map<std::uint32_t, metric_value>;

  struct hasher_ {
    auto operator()(const tag_data& d) const noexcept -> std::size_t {
      std::size_t h = size_hash_fn_(d.size());
      for (const auto& e : d)
        h ^= (elem_key_hash_fn_(e.first) * elem_mapped_hash_fn_(e.second));
      return h;
    }

   private:
    std::hash<tag_data::size_type> size_hash_fn_;
    std::hash<std::uint32_t> elem_key_hash_fn_;
    std::hash<metric_value> elem_mapped_hash_fn_;
  };

  using inverse_map = std::unordered_map<tag_data, std::uint32_t, hasher_>;

  auto make_transform_fn_() const {
    return [this](const tag_data::value_type& elem) {
      return std::pair<const std::string_view, const metric_value&>(
          str_tbl_[elem.first],
          elem.second);
    };
  }

 public:
  tag_dictionary(strval_dictionary& str_tbl);
  tag_dictionary(const tag_dictionary&) = delete;
  tag_dictionary(const tag_dictionary& y, strval_dictionary& str_tbl);
  tag_dictionary(tag_dictionary&& y, strval_dictionary& str_tbl);
  tag_dictionary& operator=(const tag_dictionary& y);
  tag_dictionary& operator=(tag_dictionary&& y);

  auto update_pending() const
  noexcept
  -> bool {
    return update_start_ < values_.size();
  }

  auto operator[](std::uint32_t idx) const -> tags;
  auto operator[](const tags& t) const -> std::uint32_t;
  auto operator[](const tags& t) -> std::uint32_t;

  auto encode_update(xdr::xdr_ostream& out) -> void;
  auto decode_update(xdr::xdr_istream& in) -> void;

 private:
  auto make_inverse_() const -> inverse_map;

  strval_dictionary& str_tbl_;
  std::vector<tag_data> values_;
  memoid<inverse_map> inverse_;
  std::uint32_t update_start_ = 0;
};

class monsoon_dirhistory_local_ dictionary
: public dynamics
{
 public:
  explicit dictionary()
  : dynamics(),
    strval_(),
    paths_(this->strval_),
    tags_(this->strval_)
  {}

  dictionary(const dictionary& y)
  : dynamics(y),
    strval_(y.strval_),
    paths_(y.paths_, this->strval_),
    tags_(y.tags_, this->strval_)
  {}

  dictionary(dictionary&& y)
  : dynamics(std::move(y)),
    strval_(std::move(y.strval_)),
    paths_(std::move(y.paths_), this->strval_),
    tags_(std::move(y.tags_), this->strval_)
  {}

  dictionary& operator=(const dictionary& y) {
    this->dynamics::operator=(y);
    strval_ = y.strval_;
    paths_ = y.paths_;
    tags_ = y.tags_;
    return *this;
  }

  dictionary& operator=(dictionary&& y) {
    this->dynamics::operator=(std::move(y));
    strval_ = std::move(y.strval_);
    paths_ = std::move(y.paths_);
    tags_ = std::move(y.tags_);
    return *this;
  }

  auto update_pending() const
  noexcept
  -> bool {
    return strval_.update_pending()
        || paths_.update_pending()
        || tags_.update_pending();
  }

  auto sdd() const
  noexcept
  -> const strval_dictionary& {
    return strval_;
  }

  auto sdd()
  noexcept
  -> strval_dictionary& {
    return strval_;
  }

  auto pdd() const
  noexcept
  -> const path_dictionary& {
    return paths_;
  }

  auto pdd()
  noexcept
  -> path_dictionary& {
    return paths_;
  }

  auto tdd() const
  noexcept
  -> const tag_dictionary& {
    return tags_;
  }

  auto tdd()
  noexcept
  -> tag_dictionary& {
    return tags_;
  }

  auto encode_update(xdr::xdr_ostream& out)
  -> void {
    // Pre-compute everything but sdd, since their computation may modify sdd.
    // They have to be written out after sdd.
    xdr::xdr_bytevector_ostream<> pre_computed;

    paths_.encode_update(pre_computed);
    tags_.encode_update(pre_computed);
    strval_.encode_update(out);
    pre_computed.copy_to(out);
  }

  auto decode_update(xdr::xdr_istream& in)
  -> void {
    strval_.decode_update(in);
    paths_.decode_update(in);
    tags_.decode_update(in);
  }

 private:
  strval_dictionary strval_;
  path_dictionary paths_;
  tag_dictionary tags_;
};

using dictionary_delta = dictionary;


} /* namespace monsoon::history::v2 */

#endif /* V2_DICTIONARY_H */
