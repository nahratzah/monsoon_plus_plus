#ifndef MONSOON_EXPRESSIONS_SELECTOR_H
#define MONSOON_EXPRESSIONS_SELECTOR_H

#include <monsoon/expr_export_.h>
#include <monsoon/expression.h>
#include <optional>
#include <iosfwd>

namespace monsoon {
namespace expressions {


class monsoon_expr_export_ path_matcher {
  friend std::ostream& operator<<(std::ostream&, const path_matcher&);

 public:
  bool operator()(const simple_group&) const;
  bool operator()(const metric_name&) const;

 private:
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

#endif /* MONSOON_EXPRESSIONS_SELECTOR_H */
