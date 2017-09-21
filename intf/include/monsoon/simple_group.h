#ifndef MONSOON_SIMPLE_GROUP_H
#define MONSOON_SIMPLE_GROUP_H

#include <initializer_list>
#include <iosfwd>
#include <string>
#include <vector>

namespace monsoon {


class simple_group {
 public:
  using path_type = std::vector<std::string>;
  using iterator = path_type::const_iterator;

  simple_group() = default;
  simple_group(const simple_group&) = default;
  simple_group(simple_group&&) noexcept;
  simple_group& operator=(const simple_group&) = default;
  simple_group& operator=(simple_group&&) noexcept;

  simple_group(std::initializer_list<const char*>);
  simple_group(std::initializer_list<std::string>);
  template<typename Iter> simple_group(Iter, Iter);

  const path_type& get_path() const noexcept;
  iterator begin() const noexcept;
  iterator end() const noexcept;

  bool operator==(const simple_group&) const noexcept;
  bool operator!=(const simple_group&) const noexcept;
  bool operator<(const simple_group&) const noexcept;
  bool operator>(const simple_group&) const noexcept;
  bool operator<=(const simple_group&) const noexcept;
  bool operator>=(const simple_group&) const noexcept;

  std::string config_string() const;

 private:
  path_type path_;
};

std::ostream& operator<<(std::ostream&, const simple_group&);


} /* namespace monsoon */


namespace std {


template<>
struct hash<monsoon::simple_group> {
  using argument_type = const monsoon::simple_group&;
  using result_type = size_t;

  size_t operator()(const monsoon::simple_group&) const noexcept;
};


} /* namespace std */

#include "simple_group-inl.h"

#endif /* MONSOON_SIMPLE_GROUP_H */
