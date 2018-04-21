#include "metric_table.h"
#include "bitset.h"
#include "xdr_primitives.h"
#include <iterator>

namespace monsoon::history::v2 {
namespace {


template<typename T> struct serialization_fn;

template<>
struct serialization_fn<std::int16_t> {
  static auto decode(xdr::xdr_istream& in) -> std::int16_t {
    return in.get_int16();
  }

  static auto encode(xdr::xdr_ostream& out, std::int16_t v) -> void {
    out.put_int16(v);
  }
};

template<>
struct serialization_fn<std::int32_t> {
  static auto decode(xdr::xdr_istream& in) -> std::int32_t {
    return in.get_int32();
  }

  static auto encode(xdr::xdr_ostream& out, std::int32_t v) -> void {
    out.put_int32(v);
  }
};

template<>
struct serialization_fn<std::int64_t> {
  static auto decode(xdr::xdr_istream& in) -> std::int64_t {
    return in.get_int64();
  }

  static auto encode(xdr::xdr_ostream& out, std::int64_t v) -> void {
    out.put_int64(v);
  }
};

template<>
struct serialization_fn<double> {
  static auto decode(xdr::xdr_istream& in) -> double {
    return in.get_flt64();
  }

  static auto encode(xdr::xdr_ostream& out, double v) -> void {
    out.put_flt64(v);
  }
};

template<>
struct serialization_fn<histogram> {
  static auto decode(xdr::xdr_istream& in) -> histogram {
    return decode_histogram(in);
  }

  static auto encode(xdr::xdr_ostream& out, histogram v) -> void {
    encode_histogram(out, v);
  }
};

template<typename ValueIter>
class mt_data_iterator {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = std::pair<bitset::size_type, typename std::iterator_traits<ValueIter>::value_type>;
  using reference = std::pair<bitset::size_type, typename std::iterator_traits<ValueIter>::reference>;
  using pointer = void;
  using difference_type = bitset::const_iterator::difference_type;

  mt_data_iterator() = default;

  mt_data_iterator(
      bitset::const_iterator presence_iter,
      bitset::const_iterator presence_begin,
      bitset::const_iterator presence_end,
      ValueIter value_iter,
      ValueIter value_end)
  : presence_iter_(std::find(presence_iter, presence_end, true)),
    presence_begin_(presence_begin),
    presence_end_(presence_end),
    value_iter_(value_iter),
    value_end_(value_end)
  {}

  auto operator++() -> mt_data_iterator& {
    assert(!at_end_());

    ++value_iter_;
    presence_iter_ = std::find(std::next(presence_iter_), presence_end_, true);
    return *this;
  }

  auto operator++(int) -> mt_data_iterator {
    mt_data_iterator copy = *this;
    ++*this;
    return copy;
  }

  auto operator*() -> reference {
    assert(!at_end_());

    auto index = presence_iter_ - presence_begin_;
    assert(index >= 0);
    return reference(bitset::size_type(index), *value_iter_);
  }

  auto operator==(const mt_data_iterator& y) const -> bool {
    if (at_end_() && y.at_end_()) return true;
    return (value_iter_ == y.value_iter_);
  }

  auto operator!=(const mt_data_iterator& y) const -> bool {
    return !(*this == y);
  }

 private:
  bool at_end_() const noexcept {
    return presence_iter_ == presence_end_ || value_iter_ == value_end_;
  }

  bitset::const_iterator presence_iter_, presence_begin_, presence_end_;
  ValueIter value_iter_, value_end_;
};

template<>
class mt_data_iterator<void> {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = bitset::size_type;
  using reference = value_type;
  using pointer = void;
  using difference_type = bitset::const_iterator::difference_type;

  mt_data_iterator() = default;

  mt_data_iterator(
      bitset::const_iterator presence_iter,
      bitset::const_iterator presence_begin,
      bitset::const_iterator presence_end)
  : presence_iter_(std::find(presence_iter, presence_end, true)),
    presence_begin_(presence_begin),
    presence_end_(presence_end)
  {}

  auto operator++() -> mt_data_iterator& {
    assert(!at_end_());

    presence_iter_ = std::find(std::next(presence_iter_), presence_end_, true);
    return *this;
  }

  auto operator++(int) -> mt_data_iterator {
    mt_data_iterator copy = *this;
    ++*this;
    return copy;
  }

  auto operator*() -> reference {
    assert(!at_end_());

    auto index = presence_iter_ - presence_begin_;
    assert(index >= 0);
    return reference(bitset::size_type(index));
  }

  auto operator==(const mt_data_iterator& y) const -> bool {
    if (at_end_() && y.at_end_()) return true;
    return false;
  }

  auto operator!=(const mt_data_iterator& y) const -> bool {
    return !(*this == y);
  }

 private:
  bool at_end_() const noexcept {
    return presence_iter_ == presence_end_;
  }

  bitset::const_iterator presence_iter_, presence_begin_, presence_end_;
};

template<typename T>
class mt_data {
 public:
  using const_iterator = mt_data_iterator<typename std::vector<T>::const_iterator>;
  using iterator = const_iterator;

  auto decode(xdr::xdr_istream& in)
  -> void {
    presence.decode(in);
    values.clear();
    in.get_collection(&serialization_fn<T>::decode, values);
  }

  auto encode(xdr::xdr_ostream& out) const
  -> void {
    presence.encode(out);
    out.put_collection(
        &serialization_fn<T>::encode,
        values.cbegin(), values.cend());
  }

  auto size() const noexcept
  -> bitset::size_type {
    return presence.size();
  }

  auto begin() const -> const_iterator {
    return const_iterator(presence.begin(), presence.begin(), presence.end(), values.begin(), values.end());
  }

  auto end() const -> const_iterator {
    return const_iterator(presence.end(), presence.begin(), presence.end(), values.end(), values.end());
  }

 private:
  bitset presence;
  std::vector<T> values;
};

template<>
class mt_data<std::string_view> {
 public:
  using const_iterator = mt_data_iterator<std::vector<std::string_view>::const_iterator>;
  using iterator = const_iterator;

  auto decode(xdr::xdr_istream& in, const strval_dictionary& dict)
  -> void {
    presence.decode(in);
    values.clear();
    in.get_collection(
        [&dict](xdr::xdr_istream& in) -> std::string_view {
          return dict[in.get_uint32()];
        },
        values);
  }

  auto encode(xdr::xdr_ostream& out, strval_dictionary& dict) const
  -> void {
    presence.encode(out);
    out.put_collection(
        [&dict](xdr::xdr_ostream& out, std::string_view v) {
          out.put_uint32(dict[v]);
        },
        values.cbegin(), values.cend());
  }

  auto size() const noexcept
  -> bitset::size_type {
    return presence.size();
  }

  auto begin() const -> const_iterator {
    return const_iterator(presence.begin(), presence.begin(), presence.end(), values.begin(), values.end());
  }

  auto end() const -> const_iterator {
    return const_iterator(presence.end(), presence.begin(), presence.end(), values.end(), values.end());
  }

 private:
  bitset presence;
  std::vector<std::string_view> values;
};

template<>
class mt_data<bool> {
 public:
  using const_iterator = mt_data_iterator<bitset::const_iterator>;
  using iterator = const_iterator;

  auto decode(xdr::xdr_istream& in)
  -> void {
    presence.decode(in);
    values.decode(in);
  }

  auto encode(xdr::xdr_ostream& out) const
  -> void {
    presence.encode(out);
    values.encode(out);
  }

  auto size() const noexcept
  -> bitset::size_type {
    return presence.size();
  }

  auto begin() const -> const_iterator {
    return const_iterator(presence.begin(), presence.begin(), presence.end(), values.begin(), values.end());
  }

  auto end() const -> const_iterator {
    return const_iterator(presence.end(), presence.begin(), presence.end(), values.end(), values.end());
  }

 private:
  bitset presence;
  bitset values;
};

template<>
class mt_data<void> {
 public:
  using const_iterator = mt_data_iterator<std::vector<metric_value>::const_iterator>;
  using iterator = const_iterator;

  auto decode(xdr::xdr_istream& in, const strval_dictionary& dict)
  -> void {
    presence.decode(in);
    values.clear();
    in.get_collection(
        [&dict](xdr::xdr_istream& in) {
          return decode_metric_value(in, dict);
        },
        values);
  }

  auto encode(xdr::xdr_ostream& out, strval_dictionary& dict) const
  -> void {
    presence.encode(out);
    out.put_collection(
        [&dict](xdr::xdr_ostream& out, const metric_value& v) {
          encode_metric_value(out, v, dict);
        },
        values.cbegin(), values.cend());
  }

  auto size() const noexcept
  -> bitset::size_type {
    return presence.size();
  }

  auto begin() const -> const_iterator {
    return const_iterator(presence.begin(), presence.begin(), presence.end(), values.begin(), values.end());
  }

  auto end() const -> const_iterator {
    return const_iterator(presence.end(), presence.begin(), presence.end(), values.end(), values.end());
  }

 private:
  bitset presence;
  std::vector<metric_value> values;
};

template<>
class mt_data<metric_value::empty> {
 public:
  using const_iterator = mt_data_iterator<void>;
  using iterator = const_iterator;

  auto decode(xdr::xdr_istream& in)
  -> void {
    presence.decode(in);
  }

  auto encode(xdr::xdr_ostream& out) const
  -> void {
    presence.encode(out);
  }

  auto size() const noexcept
  -> bitset::size_type {
    return presence.size();
  }

  auto begin() const -> const_iterator {
    return const_iterator(presence.begin(), presence.begin(), presence.end());
  }

  auto end() const -> const_iterator {
    return const_iterator(presence.end(), presence.begin(), presence.end());
  }

 private:
  bitset presence;
};

template<typename T>
auto decode_mt_data(xdr::xdr_istream& in)
-> mt_data<T> {
  mt_data<T> result;
  result.decode(in);
  return result;
}

template<typename T>
auto decode_mt_data(xdr::xdr_istream& in, const strval_dictionary& dict)
-> mt_data<T> {
  mt_data<T> result;
  result.decode(in, dict);
  return result;
}

template<typename A, typename T>
auto decode_apply(std::vector<std::optional<metric_value>, A>& c, const mt_data<T>& x)
-> void {
  c.reserve(x.size());

  std::for_each(
      x.begin(), x.end(),
      [&c](const auto& x) {
        if (c.size() <= x.first) c.resize(x.first + 1u);
        c[x.first].emplace(x.second);
      });
}

template<typename A>
auto decode_apply(std::vector<std::optional<metric_value>, A>& c, const mt_data<metric_value::empty>& x)
-> void {
  c.reserve(x.size());

  std::for_each(
      x.begin(), x.end(),
      [&c](const auto& x) {
        if (c.size() <= x) c.resize(x + 1u);
        c[x].emplace(); // Default constructor of metric value.
      });
}


} /* namespace monsoon::history::v2::<unnamed> */


metric_table::~metric_table() noexcept {}

auto metric_table::decode(xdr::xdr_istream& in, const std::shared_ptr<const strval_dictionary>& dict)
-> void {
  data_.clear();
  decode_apply(data_, decode_mt_data<bool>(in));
  decode_apply(data_, decode_mt_data<std::int16_t>(in));
  decode_apply(data_, decode_mt_data<std::int32_t>(in));
  decode_apply(data_, decode_mt_data<std::int64_t>(in));
  decode_apply(data_, decode_mt_data<double>(in));
  decode_apply(data_, decode_mt_data<std::string_view>(in, *dict));
  decode_apply(data_, decode_mt_data<histogram>(in));
  decode_apply(data_, decode_mt_data<metric_value::empty>(in));
  decode_apply(data_, decode_mt_data<void>(in, *dict));
}


} /* namespace monsoon::history::v2 */
