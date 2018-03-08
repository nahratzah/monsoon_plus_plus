#include <monsoon/simple_group.h>
#include <monsoon/grammar/intf/rules.h>
#include <stdexcept>

namespace monsoon {


simple_group simple_group::parse(std::string_view s) {
  std::string_view::iterator parse_end = s.begin();

  grammar::ast::simple_path_lit_expr result;
  bool r = grammar::x3::phrase_parse(
      parse_end, s.end(),
      grammar::parser::simple_path_lit,
      grammar::x3::space,
      result);
  if (r && parse_end == s.end())
    return result;
  throw std::invalid_argument("invalid expression");
}


} /* namespace monsoon */
