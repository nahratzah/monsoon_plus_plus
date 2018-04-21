#include "metric_table.h"
#include "bitset.h"
#include "dictionary.h"
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

  auto push_absence()
  -> void {
    presence.push_back(false);
  }

  auto push_presence(const T& v)
  -> void {
    presence.push_back(true);
    values.push_back(v);
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

  auto push_absence()
  -> void {
    presence.push_back(false);
  }

  auto push_presence(const std::string_view& v)
  -> void {
    presence.push_back(true);
    values.push_back(v);
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

  auto push_absence()
  -> void {
    presence.push_back(false);
  }

  auto push_presence(const bool& v)
  -> void {
    presence.push_back(true);
    values.push_back(v);
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

  auto push_absence()
  -> void {
    presence.push_back(false);
  }

  auto push_presence(const metric_value& v)
  -> void {
    presence.push_back(true);
    values.push_back(v);
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

  auto push_absence()
  -> void {
    presence.push_back(false);
  }

  auto push_presence([[maybe_unused]] const metric_value::empty& v)
  -> void {
    presence.push_back(true);
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


class mt_enc {
 public:
  using mt_enc_data = std::tuple<
      mt_data<bool>,
      mt_data<std::int16_t>,
      mt_data<std::int32_t>,
      mt_data<std::int64_t>,
      mt_data<double>,
      mt_data<std::string_view>,
      mt_data<histogram>,
      mt_data<metric_value::empty>,
      mt_data<void>>;

  mt_enc() = default;
  mt_enc(const mt_enc&) = delete;

  auto push_back(const std::optional<metric_value>& mv) -> void {
    if (mv.has_value())
      push_back_(*mv);
    else
      push_absence();
  }

  auto push_back_(const metric_value& mv) -> void {
    std::visit(
        overload(
            std::ref(*this),
            [this, &mv]([[maybe_unused]] const auto& unrecognized) {
              push_other(mv);
            }),
        mv.get());
  }

  auto push_other(const metric_value& v) -> void {
    std::get<mt_data<bool>>(data).push_absence();
    std::get<mt_data<std::int16_t>>(data).push_absence();
    std::get<mt_data<std::int32_t>>(data).push_absence();
    std::get<mt_data<std::int64_t>>(data).push_absence();
    std::get<mt_data<double>>(data).push_absence();
    std::get<mt_data<std::string_view>>(data).push_absence();
    std::get<mt_data<histogram>>(data).push_absence();
    std::get<mt_data<metric_value::empty>>(data).push_absence();
    std::get<mt_data<void>>(data).push_presence(v); // emit

    assert(invariant());
  }

  auto operator()(const metric_value::empty& v) -> void {
    std::get<mt_data<bool>>(data).push_absence();
    std::get<mt_data<std::int16_t>>(data).push_absence();
    std::get<mt_data<std::int32_t>>(data).push_absence();
    std::get<mt_data<std::int64_t>>(data).push_absence();
    std::get<mt_data<double>>(data).push_absence();
    std::get<mt_data<std::string_view>>(data).push_absence();
    std::get<mt_data<histogram>>(data).push_absence();
    std::get<mt_data<metric_value::empty>>(data).push_presence(v); // emit
    std::get<mt_data<void>>(data).push_absence();

    assert(invariant());
  }

  auto operator()(const bool& v) -> void {
    std::get<mt_data<bool>>(data).push_presence(v); // emit
    std::get<mt_data<std::int16_t>>(data).push_absence();
    std::get<mt_data<std::int32_t>>(data).push_absence();
    std::get<mt_data<std::int64_t>>(data).push_absence();
    std::get<mt_data<double>>(data).push_absence();
    std::get<mt_data<std::string_view>>(data).push_absence();
    std::get<mt_data<histogram>>(data).push_absence();
    std::get<mt_data<metric_value::empty>>(data).push_absence();
    std::get<mt_data<void>>(data).push_absence();

    assert(invariant());
  }

  auto operator()(const metric_value::signed_type& v) -> void {
    std::get<mt_data<bool>>(data).push_absence();

    if (v >= std::numeric_limits<std::int16_t>::min()
        && v <= std::numeric_limits<std::int16_t>::max()) {
      std::get<mt_data<std::int16_t>>(data).push_presence(v); // emit
      std::get<mt_data<std::int32_t>>(data).push_absence();
      std::get<mt_data<std::int64_t>>(data).push_absence();
      std::get<mt_data<double>>(data).push_absence();
    } else if (v >= std::numeric_limits<std::int32_t>::min()
        && v <= std::numeric_limits<std::int32_t>::max()) {
      std::get<mt_data<std::int16_t>>(data).push_absence();
      std::get<mt_data<std::int32_t>>(data).push_presence(v); // emit
      std::get<mt_data<std::int64_t>>(data).push_absence();
      std::get<mt_data<double>>(data).push_absence();
    } else if (v >= std::numeric_limits<std::int64_t>::min()
        && v <= std::numeric_limits<std::int64_t>::max()) {
      std::get<mt_data<std::int16_t>>(data).push_absence();
      std::get<mt_data<std::int32_t>>(data).push_absence();
      std::get<mt_data<std::int64_t>>(data).push_presence(v); // emit
      std::get<mt_data<double>>(data).push_absence();
    } else { // Should never happen.
      std::get<mt_data<std::int16_t>>(data).push_absence();
      std::get<mt_data<std::int32_t>>(data).push_absence();
      std::get<mt_data<std::int64_t>>(data).push_absence();
      std::get<mt_data<double>>(data).push_presence(v); // emit
    }

    std::get<mt_data<std::string_view>>(data).push_absence();
    std::get<mt_data<histogram>>(data).push_absence();
    std::get<mt_data<metric_value::empty>>(data).push_absence();
    std::get<mt_data<void>>(data).push_absence();

    assert(invariant());
  }

  auto operator()(const metric_value::unsigned_type& v) -> void {
    std::get<mt_data<bool>>(data).push_absence();

    if (v <= static_cast<metric_value::unsigned_type>(std::numeric_limits<std::int16_t>::max())) {
      std::get<mt_data<std::int16_t>>(data).push_presence(v); // emit
      std::get<mt_data<std::int32_t>>(data).push_absence();
      std::get<mt_data<std::int64_t>>(data).push_absence();
      std::get<mt_data<double>>(data).push_absence();
    } else if (v <= static_cast<metric_value::unsigned_type>(std::numeric_limits<std::int32_t>::max())) {
      std::get<mt_data<std::int16_t>>(data).push_absence();
      std::get<mt_data<std::int32_t>>(data).push_presence(v); // emit
      std::get<mt_data<std::int64_t>>(data).push_absence();
      std::get<mt_data<double>>(data).push_absence();
    } else if (v <= static_cast<metric_value::unsigned_type>(std::numeric_limits<std::int64_t>::max())) {
      std::get<mt_data<std::int16_t>>(data).push_absence();
      std::get<mt_data<std::int32_t>>(data).push_absence();
      std::get<mt_data<std::int64_t>>(data).push_presence(v); // emit
      std::get<mt_data<double>>(data).push_absence();
    } else { // Too large to represent as integer, store as floating point.
      std::get<mt_data<std::int16_t>>(data).push_absence();
      std::get<mt_data<std::int32_t>>(data).push_absence();
      std::get<mt_data<std::int64_t>>(data).push_absence();
      std::get<mt_data<double>>(data).push_presence(v); // emit
    }

    std::get<mt_data<std::string_view>>(data).push_absence();
    std::get<mt_data<histogram>>(data).push_absence();
    std::get<mt_data<metric_value::empty>>(data).push_absence();
    std::get<mt_data<void>>(data).push_absence();

    assert(invariant());
  }

  auto operator()(const metric_value::fp_type& v) -> void {
    std::get<mt_data<bool>>(data).push_absence();
    std::get<mt_data<std::int16_t>>(data).push_absence();
    std::get<mt_data<std::int32_t>>(data).push_absence();
    std::get<mt_data<std::int64_t>>(data).push_absence();
    std::get<mt_data<double>>(data).push_presence(v); // emit
    std::get<mt_data<std::string_view>>(data).push_absence();
    std::get<mt_data<histogram>>(data).push_absence();
    std::get<mt_data<metric_value::empty>>(data).push_absence();
    std::get<mt_data<void>>(data).push_absence();

    assert(invariant());
  }

  auto operator()(const std::string_view& v) -> void {
    std::get<mt_data<bool>>(data).push_absence();
    std::get<mt_data<std::int16_t>>(data).push_absence();
    std::get<mt_data<std::int32_t>>(data).push_absence();
    std::get<mt_data<std::int64_t>>(data).push_absence();
    std::get<mt_data<double>>(data).push_absence();
    std::get<mt_data<std::string_view>>(data).push_presence(v); // emit
    std::get<mt_data<histogram>>(data).push_absence();
    std::get<mt_data<metric_value::empty>>(data).push_absence();
    std::get<mt_data<void>>(data).push_absence();

    assert(invariant());
  }

  auto operator()(const histogram& v) -> void {
    std::get<mt_data<bool>>(data).push_absence();
    std::get<mt_data<std::int16_t>>(data).push_absence();
    std::get<mt_data<std::int32_t>>(data).push_absence();
    std::get<mt_data<std::int64_t>>(data).push_absence();
    std::get<mt_data<double>>(data).push_absence();
    std::get<mt_data<std::string_view>>(data).push_absence();
    std::get<mt_data<histogram>>(data).push_presence(v); // emit
    std::get<mt_data<metric_value::empty>>(data).push_absence();
    std::get<mt_data<void>>(data).push_absence();

    assert(invariant());
  }

  auto push_absence() -> void {
    std::get<mt_data<bool>>(data).push_absence();
    std::get<mt_data<std::int16_t>>(data).push_absence();
    std::get<mt_data<std::int32_t>>(data).push_absence();
    std::get<mt_data<std::int64_t>>(data).push_absence();
    std::get<mt_data<double>>(data).push_absence();
    std::get<mt_data<std::string_view>>(data).push_absence();
    std::get<mt_data<histogram>>(data).push_absence();
    std::get<mt_data<metric_value::empty>>(data).push_absence();
    std::get<mt_data<void>>(data).push_absence();

    assert(invariant());
  }

  auto write(xdr::xdr_ostream& out, strval_dictionary& dict) const
  -> void {
    assert(invariant());

    std::get<mt_data<bool>>(data).encode(out);
    std::get<mt_data<std::int16_t>>(data).encode(out);
    std::get<mt_data<std::int32_t>>(data).encode(out);
    std::get<mt_data<std::int64_t>>(data).encode(out);
    std::get<mt_data<double>>(data).encode(out);
    std::get<mt_data<std::string_view>>(data).encode(out, dict);
    std::get<mt_data<histogram>>(data).encode(out);
    std::get<mt_data<metric_value::empty>>(data).encode(out);
    std::get<mt_data<void>>(data).encode(out, dict);
  }

 private:
  auto invariant() const -> bool {
    return invariant(std::make_index_sequence<std::tuple_size_v<mt_enc_data>>());
  }

  template<std::size_t... Idx>
  auto invariant([[maybe_unused]] std::index_sequence<Idx...> indices) const
  -> std::enable_if_t<sizeof...(Idx) < 2, bool> {
    return true;
  }

  template<std::size_t Idx0, std::size_t Idx1, std::size_t... Idx>
  auto invariant([[maybe_unused]] std::index_sequence<Idx0, Idx1, Idx...> indices) const
  -> bool {
    return std::get<Idx0>(data).size() == std::get<Idx1>(data).size()
        && invariant(std::index_sequence<Idx1, Idx...>());
  }

  mt_enc_data data;
};


} /* namespace monsoon::history::v2::<unnamed> */


metric_table::~metric_table() noexcept {}

auto metric_table::from_xdr(std::shared_ptr<group_table> parent, xdr::xdr_istream& in, const dictionary& dict)
-> std::shared_ptr<metric_table> {
  std::shared_ptr<metric_table> tbl = std::make_shared<metric_table>(std::move(parent));
  tbl->decode(in, dict);
  return tbl;
}

auto metric_table::decode(xdr::xdr_istream& in, const dictionary& dict)
-> void {
  data_.clear();
  decode_apply(data_, decode_mt_data<bool>(in));
  decode_apply(data_, decode_mt_data<std::int16_t>(in));
  decode_apply(data_, decode_mt_data<std::int32_t>(in));
  decode_apply(data_, decode_mt_data<std::int64_t>(in));
  decode_apply(data_, decode_mt_data<double>(in));
  decode_apply(data_, decode_mt_data<std::string_view>(in, dict.sdd()));
  decode_apply(data_, decode_mt_data<histogram>(in));
  decode_apply(data_, decode_mt_data<metric_value::empty>(in));
  decode_apply(data_, decode_mt_data<void>(in, dict.sdd()));
}

auto metric_table::encode(xdr::xdr_ostream& out, dictionary& dict) -> void {
  mt_enc enc;
  std::for_each(
      data_.begin(), data_.end(),
      [&enc](const std::optional<metric_value>& v) { enc.push_back(v); });
  enc.write(out, dict.sdd());
}


} /* namespace monsoon::history::v2 */
