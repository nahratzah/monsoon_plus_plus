#include <monsoon/path_matcher.h>
#include <ostream>
#include <sstream>
#include <monsoon/overload.h>
#include <monsoon/config_support.h>

namespace monsoon {


template<typename MatchIter, typename ValIter>
bool monsoon_expr_local_ do_match(
    MatchIter m_b, MatchIter m_e,
    ValIter val_b, ValIter val_e) {
  using literal = path_matcher::literal;
  using wildcard = path_matcher::wildcard;
  using double_wildcard = path_matcher::double_wildcard;

  while (m_b != m_e) {
    // Visitor tests if the *m_b element matches.
    // False indicates the ValIter range cannot possibly
    // match.
    // True indicates that val_b is matched with m_b.
    const bool continue_search = std::visit(
        overload(
            [&val_b, &val_e](const literal& lit) {
              if (val_b != val_e && lit == *val_b) {
                ++val_b;
                return true;
              } else {
                return false;
              }
            },
            [&val_b, &val_e](const wildcard&) {
              if (val_b != val_e) {
                ++val_b;
                return true;
              } else {
                return false;
              }
            },
            [&val_b, &val_e, m_b, m_e](const double_wildcard&) { // Use recursion.
              auto greedy_val_b = val_e;
              while (greedy_val_b-- != val_b) {
                if (do_match(std::next(m_b), m_e, greedy_val_b, val_e)) {
                  val_b = val_e;
                  return true;
                }
              }
              return false;
            }),
        *m_b++);

    if (!continue_search) return false;
  }

  return val_b == val_e;
}

bool path_matcher::operator()(const simple_group& g) const {
  return do_match(matcher_.begin(), matcher_.end(), g.begin(), g.end());
}

bool path_matcher::operator()(const metric_name& m) const {
  return do_match(matcher_.begin(), matcher_.end(), m.begin(), m.end());
}

void path_matcher::push_back_literal(literal&& lit) {
  matcher_.emplace_back(std::in_place_type<literal>, std::move(lit));
}

void path_matcher::push_back_literal(std::string_view lit) {
  matcher_.emplace_back(std::in_place_type<literal>, lit);
}

void path_matcher::push_back_wildcard() {
  if (!matcher_.empty() &&
      std::holds_alternative<double_wildcard>(matcher_.back())) {
    matcher_.emplace(std::prev(matcher_.end()), std::in_place_type<wildcard>);
  } else {
    matcher_.emplace_back(std::in_place_type<wildcard>);
  }
}

void path_matcher::push_back_double_wildcard() {
  if (matcher_.empty() ||
      !std::holds_alternative<double_wildcard>(matcher_.back())) {
    matcher_.emplace_back(std::in_place_type<double_wildcard>);
  }
}


auto operator<<(std::ostream& out, const path_matcher& pm) -> std::ostream& {
  using literal = path_matcher::literal;
  using wildcard = path_matcher::wildcard;
  using double_wildcard = path_matcher::double_wildcard;

  bool first = true;
  for (const auto& v : pm) {
    if (!std::exchange(first, false)) out << ".";
    std::visit(
        overload(
            [&out](const literal& lit) {
              out << maybe_quote_identifier(lit);
            },
            [&out](const wildcard&) {
              out << "*";
            },
            [&out](const double_wildcard&) {
              out << "**";
            }),
        v);
  }
  return out;
}

std::string to_string(const path_matcher& pm) {
  return (std::ostringstream() << pm).str();
}


} /* namespace monsoon */
