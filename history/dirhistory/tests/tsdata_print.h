#ifndef TSDATA_PRINT_H
#define TSDATA_PRINT_H

#include <ostream>
#include <cstdint>
#include <tuple>
#include <type_traits>
#include <monsoon/time_series.h>

namespace std {


// HACK: print collection of time series.
template<typename C>
auto operator<< (ostream& out, const C& c)
-> std::enable_if_t<std::is_same<monsoon::time_series, typename C::value_type>::value
                 || std::is_same<monsoon::time_series_value, typename C::value_type>::value
                 || std::is_same<monsoon::group_name, typename C::value_type>::value
                 || std::is_same<monsoon::simple_group, typename C::value_type>::value,
                    ostream&>
{
  auto i = c.begin();
  auto e = c.end();

  out << "[";
  if (i != e) out << " " << *i++;
  while (i != e) out << ", " << *i++;
  out << (c.empty() ? "]" : " ]");
  return out;
}

// HACK: print collection of time series.
template<typename C>
auto operator<< (ostream& out, const C& c)
-> std::enable_if_t<std::is_same<std::tuple<monsoon::group_name, monsoon::metric_name>, typename C::value_type>::value
                 || std::is_same<std::tuple<monsoon::simple_group, monsoon::metric_name>, typename C::value_type>::value,
                    ostream&>
{
  auto i = c.begin();
  auto e = c.end();

  out << "[";
  if (i != e) {
    auto v = *i++;
    out << " " << std::get<0>(v) << "::" << std::get<1>(v);
  }
  while (i != e) {
    auto v = *i++;
    out << ", " << std::get<0>(v) << "::" << std::get<1>(v);
  }
  out << (c.empty() ? "]" : " ]");
  return out;
}

// HACK: print collection of time series.
template<typename C>
auto operator<< (ostream& out, const C& c)
-> std::enable_if_t<std::is_same<monsoon::metric_name, typename C::value_type::first_type>::value
                 || std::is_same<monsoon::metric_value, typename C::value_type::second_type>::value,
                    ostream&>
{
  auto i = c.begin();
  auto e = c.end();

  out << "[";
  if (i != e) {
    out << " " << i->first << "=" << i->second;
    ++i;
  }
  while (i != e) {
    out << " " << i->first << "=" << i->second;
    ++i;
  }
  out << (c.empty() ? "]" : " ]");
  return out;
}

inline auto operator<< (ostream& out, const monsoon::time_series& ts) -> ostream& {
  out << ts.get_time() << " -> " << ts.get_data();
  return out;
}

inline auto operator<< (ostream& out, const monsoon::time_series_value& tsv) -> ostream& {
  out << tsv.get_name() << ": " << tsv.get_metrics();
  return out;
}

template<size_t Idx, typename... T>
auto print_tuple(ostream& out, const std::tuple<T...>& t)
-> std::enable_if_t<(Idx < sizeof...(T)), void> {
  if (Idx > 0) out << ", ";
  out << get<Idx>(t);
  print_tuple<Idx + 1>(out, t);
}

template<size_t Idx, typename... T>
auto print_tuple(ostream& out, const std::tuple<T...>& t)
-> std::enable_if_t<(Idx == sizeof...(T)), void> {
  return;
}

template<typename... T>
auto operator<< (ostream& out, const std::tuple<T...>& t) -> ostream& {
  out << "(";
  print_tuple<0>(out, t);
  out << ")";
  return out;
}


} /* namespace std */

#endif /* TSDATA_PRINT_H */
