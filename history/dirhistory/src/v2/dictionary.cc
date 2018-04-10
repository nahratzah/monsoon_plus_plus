#include "dictionary.h"
#include "encdec.h"
#include <stdexcept>
#include <boost/iterator/transform_iterator.hpp>

namespace monsoon::history::v2 {


strval_dictionary::strval_dictionary()
: inverse_(std::bind(&strval_dictionary::make_inverse_, this))
{}

strval_dictionary::strval_dictionary(const strval_dictionary& y)
: values_(y.values_),
  inverse_(std::bind(&strval_dictionary::make_inverse_, this)),
  update_start_(y.update_start_)
{}

strval_dictionary::strval_dictionary(strval_dictionary&& y)
: values_(std::move(y.values_)),
  inverse_(std::bind(&strval_dictionary::make_inverse_, this)),
  update_start_(y.update_start_)
{}

strval_dictionary& strval_dictionary::operator=(const strval_dictionary& y) {
  inverse_.reset();
  values_ = y.values_;
  update_start_ = y.update_start_;
  return *this;
}

strval_dictionary& strval_dictionary::operator=(strval_dictionary&& y) {
  inverse_.reset();
  values_ = std::move(y.values_);
  update_start_ = y.update_start_;
  return *this;
}

auto strval_dictionary::operator[](std::uint32_t idx) const
-> std::string_view {
  return values_.at(idx);
}

auto strval_dictionary::operator[](std::string_view s) const
-> std::uint32_t {
  auto pos = inverse_->find(s);
  if (pos == inverse_->end()) throw std::out_of_range("string not present");
  return pos->second;
}

auto strval_dictionary::operator[](std::string_view s)
-> std::uint32_t {
  auto pos = inverse_->find(s);
  if (pos != inverse_->end()) return pos->second;

  const bool will_invalidate_refs = (values_.size() == values_.capacity());
  values_.emplace_back(s.begin(), s.end());

  if (will_invalidate_refs) {
    inverse_.reset();
  } else {
    try {
      inverse_->emplace(values_.back(), values_.size() - 1u);
    } catch (...) {
      inverse_.reset();
    }
  }
  return values_.size() - 1u;
}

auto strval_dictionary::encode_update(xdr::xdr_ostream& out) -> void {
  out.put_uint32(update_start_);
  out.put_collection(
      [](xdr::xdr_ostream& out, std::string_view v) {
        out.put_string(v);
      },
      values_.begin() + update_start_,
      values_.end());
  update_start_ = values_.size();
}

auto strval_dictionary::decode_update(xdr::xdr_istream& in) -> void {
  inverse_.reset();

  const std::uint32_t offset = in.get_uint32();
  if (offset != values_.size())
    throw xdr::xdr_exception("dictionary updates must be contiguous");

  try {
    in.get_collection(
        [](xdr::xdr_istream& in) {
          return in.get_string();
        },
        values_);
    if (values_.size() > 0xffffffffU)
      throw xdr::xdr_exception("dictionary too large");
  } catch (...) {
    values_.resize(offset);
    throw;
  }

  update_start_ = values_.size();
}

auto strval_dictionary::make_inverse_() const
-> inverse_map {
  inverse_map result;
  result.reserve(values_.size());

  std::uint32_t idx = 0;
  for (std::string_view i : values_)
    result.emplace(i, idx++);
  return result;
}


path_dictionary::proxy::operator metric_name() const {
  using boost::iterators::make_transform_iterator;

  return metric_name(
      make_transform_iterator(self_.values_[ref_].begin(), make_transform_fn_()),
      make_transform_iterator(self_.values_[ref_].end(), make_transform_fn_()));
}

path_dictionary::proxy::operator simple_group() const {
  using boost::iterators::make_transform_iterator;

  return simple_group(
      make_transform_iterator(self_.values_[ref_].begin(), make_transform_fn_()),
      make_transform_iterator(self_.values_[ref_].end(), make_transform_fn_()));
}


path_dictionary::path_dictionary(strval_dictionary& str_tbl)
: str_tbl_(str_tbl),
  inverse_(std::bind(&path_dictionary::make_inverse_, this))
{}

path_dictionary::path_dictionary(const path_dictionary& y, strval_dictionary& str_tbl)
: str_tbl_(str_tbl),
  values_(y.values_),
  inverse_(std::bind(&path_dictionary::make_inverse_, this)),
  update_start_(y.update_start_)
{}

path_dictionary::path_dictionary(path_dictionary&& y, strval_dictionary& str_tbl)
: str_tbl_(str_tbl),
  values_(std::move(y.values_)),
  inverse_(std::bind(&path_dictionary::make_inverse_, this)),
  update_start_(y.update_start_)
{}

path_dictionary& path_dictionary::operator=(const path_dictionary& y) {
  inverse_.reset();
  values_ = y.values_;
  update_start_ = y.update_start_;
  return *this;
}

path_dictionary& path_dictionary::operator=(path_dictionary&& y) {
  inverse_.reset();
  values_ = std::move(y.values_);
  update_start_ = y.update_start_;
  return *this;
}

auto path_dictionary::operator[](std::uint32_t idx) const
-> proxy {
  if (idx >= values_.size()) throw std::out_of_range("path index not in dictionary");
  return proxy(*this, idx);
}

auto path_dictionary::operator[](const path_common& pc) const
-> std::uint32_t {
  const strval_dictionary& str_tbl = str_tbl_; // Local const-reference, to propagate const-ness.

  path p;
  std::transform(
      pc.begin(),
      pc.end(),
      std::back_inserter(p),
      [&str_tbl](std::string_view s) {
        return str_tbl[s];
      });

  const auto pos = inverse_->find(p);
  if (pos == inverse_->end()) throw std::out_of_range("path not found in dictonary");
  return pos->second;
}

auto path_dictionary::operator[](const path_common& pc)
-> std::uint32_t {
  strval_dictionary& str_tbl = str_tbl_; // Local non-const-reference, to propagate mutable-ness.

  path p;
  std::transform(
      pc.begin(),
      pc.end(),
      std::back_inserter(p),
      [&str_tbl](std::string_view s) {
        return str_tbl[s];
      });

  const auto pos = inverse_->find(p);
  if (pos != inverse_->end()) return pos->second;

  values_.push_back(p);
  try {
    inverse_->emplace(std::move(p), values_.size() - 1u);
  } catch (...) {
    inverse_.reset();
  }
  return values_.size() - 1u;
}

auto path_dictionary::encode_update(xdr::xdr_ostream& out) -> void {
  out.put_uint32(update_start_);
  out.put_collection(
      [](xdr::xdr_ostream& out, const path& v) {
        out.put_collection(
            [](xdr::xdr_ostream& out, std::uint32_t v) {
              out.put_uint32(v);
            },
            v.begin(),
            v.end());
      },
      values_.begin() + update_start_,
      values_.end());
  update_start_ = values_.size();
}

auto path_dictionary::decode_update(xdr::xdr_istream& in) -> void {
  inverse_.reset();

  const std::uint32_t offset = in.get_uint32();
  if (offset != values_.size())
    throw xdr::xdr_exception("dictionary updates must be contiguous");

  try {
    in.get_collection(
        [](xdr::xdr_istream& in) {
          return in.get_collection<path>(
              [](xdr::xdr_istream& in) {
                return in.get_uint32();
              });
        },
        values_);
    if (values_.size() > 0xffffffffU)
      throw xdr::xdr_exception("dictionary too large");
  } catch (...) {
    values_.resize(offset);
    throw;
  }

  update_start_ = values_.size();
}

auto path_dictionary::make_inverse_() const
-> inverse_map {
  inverse_map result;
  result.reserve(values_.size());

  std::uint32_t idx = 0;
  for (const path& e : values_)
    result.emplace(e, idx++);
  return result;
}


tag_dictionary::tag_dictionary(strval_dictionary& str_tbl)
: str_tbl_(str_tbl),
  inverse_(std::bind(&tag_dictionary::make_inverse_, this))
{}

tag_dictionary::tag_dictionary(const tag_dictionary& y, strval_dictionary& str_tbl)
: str_tbl_(str_tbl),
  values_(y.values_),
  inverse_(std::bind(&tag_dictionary::make_inverse_, this)),
  update_start_(y.update_start_)
{}

tag_dictionary::tag_dictionary(tag_dictionary&& y, strval_dictionary& str_tbl)
: str_tbl_(str_tbl),
  values_(std::move(y.values_)),
  inverse_(std::bind(&tag_dictionary::make_inverse_, this)),
  update_start_(y.update_start_)
{}

tag_dictionary& tag_dictionary::operator=(const tag_dictionary& y) {
  inverse_.reset();
  values_ = y.values_;
  update_start_ = y.update_start_;
  return *this;
}

tag_dictionary& tag_dictionary::operator=(tag_dictionary&& y) {
  inverse_.reset();
  values_ = std::move(y.values_);
  update_start_ = y.update_start_;
  return *this;
}

auto tag_dictionary::operator[](std::uint32_t idx) const
-> tags {
  using boost::iterators::make_transform_iterator;

  const tag_data& data = values_.at(idx);
  return tags(
      make_transform_iterator(data.begin(), make_transform_fn_()),
      make_transform_iterator(data.end(), make_transform_fn_()));
}

auto tag_dictionary::operator[](const tags& t) const
-> std::uint32_t {
  const strval_dictionary& str_tbl = str_tbl_; // Local const-reference, to propagate const-ness.

  tag_data data;
  std::for_each(
      t.begin(), t.end(),
      [&str_tbl, &data](const auto& elem) {
        data.emplace(str_tbl[elem.first], elem.second);
      });

  const auto pos = inverse_->find(data);
  if (pos == inverse_->end()) throw std::out_of_range("tags not found in dictionary");
  return pos->second;
}

auto tag_dictionary::operator[](const tags& t)
-> std::uint32_t {
  strval_dictionary& str_tbl = str_tbl_; // Local non-const-reference, to propagate mutable-ness.

  tag_data data;
  std::for_each(
      t.begin(), t.end(),
      [&str_tbl, &data](const auto& elem) {
        data.emplace(str_tbl[elem.first], elem.second);
      });

  const auto pos = inverse_->find(data);
  if (pos != inverse_->end()) return pos->second;

  values_.push_back(data);
  try {
    inverse_->emplace(std::move(data), values_.size() - 1u);
  } catch (...) {
    inverse_.reset();
  }
  return values_.size() - 1u;
}

auto tag_dictionary::encode_update(xdr::xdr_ostream& out) -> void {
  out.put_uint32(update_start_);
  out.put_collection(
      [this](xdr::xdr_ostream& out, const tag_data& v) {
        // First pass: encode metric name keys.
        out.put_collection(
            [](xdr::xdr_ostream& out, const tag_data::value_type& v) {
              out.put_uint32(v.first);
            },
            v.begin(),
            v.end());
        // Second pass: encode metric values.
        out.put_collection(
            [this](xdr::xdr_ostream& out, const tag_data::value_type& v) {
              encode_metric_value(out, v.second, str_tbl_);
            },
            v.begin(),
            v.end());
      },
      values_.begin() + update_start_,
      values_.end());
  update_start_ = values_.size();
}

auto tag_dictionary::decode_update(xdr::xdr_istream& in) -> void {
  inverse_.reset();

  const std::uint32_t offset = in.get_uint32();
  if (offset != values_.size())
    throw xdr::xdr_exception("dictionary updates must be contiguous");

  try {
    in.get_collection(
        [this](xdr::xdr_istream& in) {
          const auto keys = in.get_collection<std::vector<std::uint32_t>>(
              [](xdr::xdr_istream& in) {
                return in.get_uint32();
              });
          auto keys_iter = keys.begin();

          tag_data result = in.get_collection<tag_data>(
              [this, &keys, &keys_iter](xdr::xdr_istream& in) {
                // Throw exception if there are too many values.
                if (keys_iter == keys.end())
                  throw xdr::xdr_exception("tag dictionary length mismatch");
                return std::make_pair(*keys_iter++, decode_metric_value(in, str_tbl_));
              });

          // Throw exception if there are insufficient values.
          if (keys_iter != keys.end())
            throw xdr::xdr_exception("tag dictionary length mismatch");
          return result;
        },
        values_);
    if (values_.size() > 0xffffffffU)
      throw xdr::xdr_exception("dictionary too large");
  } catch (...) {
    values_.resize(offset);
    throw;
  }

  update_start_ = values_.size();
}

auto tag_dictionary::make_inverse_() const
-> inverse_map {
  inverse_map result;
  result.reserve(values_.size());

  std::uint32_t idx = 0;
  for (const auto& e : values_)
    result.emplace(e, idx++);
  return result;
}


} /* namespace monsoon::history::v2 */
