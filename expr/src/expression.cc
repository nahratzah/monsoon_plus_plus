#include <monsoon/expression.h>
#include <monsoon/grammar/expression/rules.h>
#include <monsoon/overload.h>
#include <sstream>
#include <ostream>
#include <utility>

namespace monsoon {


expression_ptr expression::parse(std::string_view s) {
  std::string_view::iterator parse_end = s.begin();

  grammar::expression_result result;
  bool r = grammar::x3::phrase_parse(
      parse_end, s.end(),
      grammar::expression,
      grammar::x3::space_type(),
      result);
  if (r && parse_end == s.end())
    return result;
  throw std::invalid_argument("invalid expression");
}

expression::~expression() noexcept {}


std::string to_string(const expression& expr) {
  return (std::ostringstream() << expr).str();
}

auto operator<<(std::ostream& out, const expression& expr) -> std::ostream& {
  expr.do_ostream(out);
  return out;
}


bool operator==(
    const expression::scalar_emit_type& x,
    const expression::scalar_emit_type& y) noexcept {
  return x.tp == y.tp && x.data == y.data;
}

bool operator==(
    const expression::vector_emit_type& x,
    const expression::vector_emit_type& y) noexcept {
  return x.tp == y.tp && x.data == y.data;
}

auto operator<<(std::ostream& out, const expression::scalar_emit_type& v)
-> std::ostream& {
  return out
      << (v.data.index() == 0u ? "speculative" : "factual")
      << " scalar("
      << std::visit(
          [](const metric_value& v) -> const metric_value& {
            return v;
          },
          v.data)
      << " at "
      << v.tp
      << ")";
}

auto operator<<(std::ostream& out, const expression::vector_emit_type& v)
-> std::ostream& {
  out
      << (v.data.index() == 0u ? "speculative" : "factual")
      << " vector(";
  std::visit(
      overload(
          [&out](const expression::speculative_vector& v) {
            out << std::get<0>(v)
                << "="
                << std::get<1>(v);
          },
          [&out](const expression::factual_vector& v) {
            bool first = true;
            for (const auto& e : v) {
              out << (std::exchange(first, false) ? "{" : ", ")
                  << e.first
                  << ": "
                  << e.second;
            }
            out << "}";
          }),
      v.data);
  return out
      << " at "
      << v.tp
      << ")";
}


} /* namespace monsoon */
