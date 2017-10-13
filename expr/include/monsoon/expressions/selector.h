#ifndef MONSOON_EXPRESSIONS_SELECTOR_H
#define MONSOON_EXPRESSIONS_SELECTOR_H

#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <optional>
#include <iosfwd>
#include <string>
#include <variant>

namespace monsoon {
namespace expressions {


class monsoon_expr_export_ path_matcher {
 public:
  using literal = std::string;
  struct wildcard {};
  struct double_wildcard {};
  using match_element = std::variant<literal, wildcard, double_wildcard>;

 private:
  using matcher_vector = std::vector<match_element>;

 public:
  using const_iterator = matcher_vector::const_iterator;
  using iterator = const_iterator;

  path_matcher() = default;
  path_matcher(const path_matcher&);
  path_matcher& operator=(const path_matcher&);
  path_matcher(path_matcher&&) noexcept;
  path_matcher& operator=(path_matcher&&) noexcept;
  ~path_matcher() noexcept;

  bool operator()(const simple_group&) const;
  bool operator()(const metric_name&) const;

  const_iterator begin() const;
  const_iterator end() const;
  const_iterator cbegin() const;
  const_iterator cend() const;

  void push_back_literal(literal&&);
  void push_back_literal(std::string_view);
  void push_back_wildcard();
  void push_back_double_wildcard();

 private:
  matcher_vector matcher_;
};

class monsoon_expr_export_ tag_matcher {
  friend std::ostream& operator<<(std::ostream&, const tag_matcher&);

 public:
  enum comparison {
    eq,
    ne,
    lt,
    gt,
    le,
    ge
  };

  using comparison_match = std::tuple<comparison, metric_value>;
  struct presence_match {};
  struct absence_match {};
  using match_element =
      std::variant<absence_match, presence_match, comparison_match>;

 private:
  using matcher_map = std::multimap<std::string, match_element>;

 public:
  using const_iterator = matcher_map::const_iterator;
  using iterator = const_iterator;

  tag_matcher() = default;
  tag_matcher(const tag_matcher&);
  tag_matcher& operator=(const tag_matcher&);
  tag_matcher(tag_matcher&&) noexcept;
  tag_matcher& operator=(tag_matcher&&) noexcept;
  ~tag_matcher() noexcept;

  bool operator()(const tags&) const;

  const_iterator begin() const;
  const_iterator end() const;
  const_iterator cbegin() const;
  const_iterator cend() const;

  void check_comparison(std::string&&, comparison, metric_value&&);
  void check_comparison(std::string_view, comparison, const metric_value&);
  void check_presence(std::string&&);
  void check_presence(std::string_view);
  void check_absence(std::string&&);
  void check_absence(std::string_view);

 private:
  matcher_map matcher_;
};


monsoon_expr_export_
auto selector(path_matcher, path_matcher)
    -> expression_ptr;
monsoon_expr_export_
auto selector(path_matcher, tag_matcher, path_matcher)
    -> expression_ptr;
monsoon_expr_export_
auto selector(path_matcher, std::optional<tag_matcher>, path_matcher)
    -> expression_ptr;

monsoon_expr_export_
std::ostream& operator<<(std::ostream&, const path_matcher&);

monsoon_expr_export_
std::string to_string(const path_matcher&);


}} /* namespace monsoon::expressions */

#include "selector-inl.h"

#endif /* MONSOON_EXPRESSIONS_SELECTOR_H */
