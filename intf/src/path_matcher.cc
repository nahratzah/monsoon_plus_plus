#include <monsoon/path_matcher.h>
#include <cassert>
#include <ostream>
#include <sstream>
#include <monsoon/overload.h>
#include <monsoon/config_support.h>

namespace monsoon {


template<typename MatchIter, typename ValIter>
bool do_match(
    MatchIter m_b, MatchIter m_e,
    ValIter val_b, ValIter val_e) {
  using literal = path_matcher::literal;
  using wildcard = path_matcher::wildcard;
  using double_wildcard = path_matcher::double_wildcard;

  // Loop, matching exactly one element each time.
  while (m_b != m_e && !std::holds_alternative<double_wildcard>(*m_b)) {
    if (val_b == val_e) return false;

    bool match_one = std::visit(
        overload(
            [&val_b](const literal& lit) -> bool {
              return std::equal_to<std::string_view>()(lit, *val_b);
            },
            [](const wildcard&) -> bool {
              return true;
            },
            [](const double_wildcard&) -> bool {
              assert(false); // Loop stops when reaching double wildcard.
            }),
        *m_b);
    if (!match_one) return false; // Mismatch

    ++m_b;
    ++val_b;
  }

  if (m_b != m_e) {
    assert(std::holds_alternative<double_wildcard>(*m_b));

    ++m_b;
    // Greedy traversal.
    ValIter greedy_b = val_e;
    for (;;) {
      if (do_match(m_b, m_e, greedy_b, val_e)) // Recursion.
        return true;

      if (greedy_b == val_b) // Stop when all subsets have been tested.
        return false;
      --greedy_b;
    }
    // unreachable
  }

  assert(m_b == m_e); // Exhausted all matchers.
  assert(val_b == val_e); // Exhausted all elements.
  return true;
}

bool path_matcher::operator()(const simple_group& g) const {
  return do_match(matcher_.begin(), matcher_.end(), g.begin(), g.end());
}

bool path_matcher::operator()(const metric_name& m) const {
  return do_match(matcher_.begin(), matcher_.end(), m.begin(), m.end());
}

path_matcher& path_matcher::push_back_literal(literal&& lit) {
  matcher_.emplace_back(std::in_place_type<literal>, std::move(lit));
  return *this;
}

path_matcher& path_matcher::push_back_literal(std::string_view lit) {
  matcher_.emplace_back(std::in_place_type<literal>, lit);
  return *this;
}

path_matcher& path_matcher::push_back_literal(const char* lit) {
  return push_back_literal(std::string_view(lit));
}

path_matcher& path_matcher::push_back_wildcard() {
  if (!matcher_.empty() &&
      std::holds_alternative<double_wildcard>(matcher_.back())) {
    matcher_.emplace(std::prev(matcher_.end()), std::in_place_type<wildcard>);
  } else {
    matcher_.emplace_back(std::in_place_type<wildcard>);
  }
  return *this;
}

path_matcher& path_matcher::push_back_double_wildcard() {
  if (matcher_.empty() ||
      !std::holds_alternative<double_wildcard>(matcher_.back())) {
    matcher_.emplace_back(std::in_place_type<double_wildcard>);
  }
  return *this;
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
