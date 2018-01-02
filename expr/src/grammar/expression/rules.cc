#include <monsoon/grammar/expression/rules.h>

namespace monsoon {
namespace grammar {
namespace parser {


muldiv_sym::muldiv_sym() {
  add("*", ast::muldiv_enum::mul);
  add("/", ast::muldiv_enum::div);
  add("%", ast::muldiv_enum::mod);
}

addsub_sym::addsub_sym() {
  add("+", ast::addsub_enum::add);
  add("-", ast::addsub_enum::sub);
}

shift_sym::shift_sym() {
  add("<<", ast::shift_enum::left);
  add(">>", ast::shift_enum::right);
}

compare_sym::compare_sym() {
  add("<=", ast::compare_enum::le);
  add(">=", ast::compare_enum::ge);
  add("<", ast::compare_enum::lt);
  add(">", ast::compare_enum::gt);
}

equality_sym::equality_sym() {
  add("=", ast::equality_enum::eq);
  add("!=", ast::equality_enum::ne);
}


}}} /* namespace monsoon::grammar::parser */
