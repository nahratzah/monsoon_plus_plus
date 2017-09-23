#ifndef TSDATA_PRINT_H
#define TSDATA_PRINT_H

#include <ostream>
#include <type_traits>
#include <monsoon/time_series.h>

namespace std {


// HACK: print collection of time series.
template<typename C>
auto operator<< (ostream& out, const C& c)
-> std::enable_if_t<std::is_same<monsoon::time_series, typename C::value_type>::value,
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

inline auto operator<< (ostream& out, const monsoon::time_series& ts) -> ostream& {
  out << ts.get_time();
  return out;
}


} /* namespace std */

#endif /* TSDATA_PRINT_H */
