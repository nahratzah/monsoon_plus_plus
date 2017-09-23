#ifndef TSDATA_PRINT_H
#define TSDATA_PRINT_H

#include <ostream>
#include <type_traits>
#include <monsoon/time_series.h>

namespace std {


// HACK: print collection of time series.
template<typename C>
auto operator<< (ostream& out, const C& c)
-> std::enable_if_t<std::is_same<monsoon::time_series, typename C::value_type>::value
                 || std::is_same<monsoon::time_series_value, typename C::value_type>::value,
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


} /* namespace std */

#endif /* TSDATA_PRINT_H */
