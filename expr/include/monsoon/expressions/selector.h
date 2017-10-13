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
  friend std::ostream& operator<<(std::ostream&, const path_matcher&);

 public:
  using literal = std::string;
  struct wildcard {};
  struct double_wildcard {};
  using match_element = std::variant<literal, wildcard, double_wildcard>;

  path_matcher() = default;
  path_matcher(const path_matcher&);
  path_matcher& operator=(const path_matcher&);
  path_matcher(path_matcher&&) noexcept;
  path_matcher& operator=(path_matcher&&) noexcept;
  ~path_matcher() noexcept;

  bool operator()(const simple_group&) const;
  bool operator()(const metric_name&) const;

 private:
  std::vector<match_element> matcher_;
};

class monsoon_expr_export_ tag_matcher {
  friend std::ostream& operator<<(std::ostream&, const tag_matcher&);

 public:
  bool operator()(const tags&) const;

 private:
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


}} /* namespace monsoon::expressions */

#include "selector-inl.h"

#endif /* MONSOON_EXPRESSIONS_SELECTOR_H */
