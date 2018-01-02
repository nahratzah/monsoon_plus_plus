#include <monsoon/grammar/intf/rules.h>

namespace monsoon {
namespace grammar {
namespace parser {


tag_matcher_comparison_sym::tag_matcher_comparison_sym() {
  add("=", tag_matcher::eq);
  add("!=", tag_matcher::ne);
  add("<", tag_matcher::lt);
  add(">", tag_matcher::gt);
  add("<=", tag_matcher::le);
  add(">=", tag_matcher::ge);
}

const struct tag_matcher_comparison_sym tag_matcher_comparison_sym;


}}} /* namespace monsoon::grammar::parser */
