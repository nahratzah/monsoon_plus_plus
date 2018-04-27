#include <monsoon/path_common.h>
#include <monsoon/config_support.h>
#include <monsoon/cache/impl.h>
#include <monsoon/instrumentation.h>
#include <algorithm>
#include <utility>
#include <ostream>
#include <sstream>
#include <chrono>

namespace monsoon {


path_common::cache_type path_common::cache_() {
  static cache_type impl = path_common::cache_type::builder()
      .stats("path_names", cache_instrumentation())
      .build(cache_create_());
  return impl;
}

path_common::path_common()
: path_(cache_()())
{}

path_common::path_common(const path_type& p)
: path_(cache_()(p))
{}

path_common::path_common(std::initializer_list<const char*> init)
: path_common(init.begin(), init.end())
{}

path_common::path_common(std::initializer_list<std::string> init)
: path_common(init.begin(), init.end())
{}

path_common::path_common(std::initializer_list<std::string_view> init)
: path_common(init.begin(), init.end())
{}

auto path_common::operator==(const path_common& other) const noexcept
->  bool {
  return path_ == other.path_ ||
      std::equal(begin(), end(), other.begin(), other.end());
}

auto path_common::operator<(const path_common& other) const noexcept
->  bool {
  return path_ != other.path_
      && std::lexicographical_compare(begin(), end(),
                                      other.begin(), other.end());
}

auto path_common::config_string() const -> std::string {
  return (std::ostringstream() << *this).str();
}


auto operator<<(std::ostream& out, const path_common& n) -> std::ostream& {
  bool first = true;

  for (std::string_view s : n) {
    if (!std::exchange(first, false))
      out << ".";
    out << maybe_quote_identifier(s);
  }
  return out;
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::path_common>::operator()(const monsoon::path_common& v)
    const noexcept
->  size_t {
  auto s_hash = std::hash<std::string_view>();

  size_t rv = 0;
  for (std::string_view s : v)
    rv = 19u * rv + s_hash(s);
  return rv;
}


} /* namespace std */
