#include <monsoon/expressions/selector.h>
#include "../overload.h"
#include <ostream>
#include <iterator>
#include <sstream>
#include <monsoon/config_support.h>

namespace monsoon {
namespace expressions {


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

path_matcher::path_matcher(const path_matcher& o)
: matcher_(o.matcher_)
{}

auto path_matcher::operator=(const path_matcher& o) -> path_matcher& {
  matcher_ = o.matcher_;
  return *this;
}

path_matcher::~path_matcher() noexcept {}

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


tag_matcher::tag_matcher(const tag_matcher& o)
: matcher_(o.matcher_)
{}

auto tag_matcher::operator=(const tag_matcher& o) -> tag_matcher& {
  matcher_ = o.matcher_;
  return *this;
}

tag_matcher::~tag_matcher() noexcept {}

bool tag_matcher::operator()(const tags& t) const {
  for (const auto& m : matcher_) {
    const bool cmp = std::visit(
        overload(
            [&t, &m](const presence_match&) {
              return t[m.first].has_value();
            },
            [&t, &m](const absence_match&) {
              return !t[m.first].has_value();
            },
            [&t, &m](const comparison_match& cmp) {
              auto opt_mv = t[m.first];
              if (!opt_mv) return false;

              metric_value mv;
              switch (std::get<0>(cmp)) {
                case eq:
                  mv = equal(opt_mv.value(), std::get<1>(cmp));
                  break;
                case ne:
                  mv = unequal(opt_mv.value(), std::get<1>(cmp));
                  break;
                case lt:
                  mv = less(opt_mv.value(), std::get<1>(cmp));
                  break;
                case gt:
                  mv = greater(opt_mv.value(), std::get<1>(cmp));
                  break;
                case le:
                  mv = less_equal(opt_mv.value(), std::get<1>(cmp));
                  break;
                case ge:
                  mv = greater_equal(opt_mv.value(), std::get<1>(cmp));
                  break;
              }
              return mv.as_bool().value_or(false);
            }),
        m.second);

    if (!cmp) return false;
  }
  return true;
}

void tag_matcher::check_comparison(std::string&& tagname, comparison cmp,
    metric_value&& tagvalue) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(tagname)),
      std::forward_as_tuple(
          std::in_place_type<comparison_match>,
          std::move(cmp),
          std::move(tagvalue)));
}

void tag_matcher::check_comparison(std::string_view tagname, comparison cmp,
    const metric_value& tagvalue) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(tagname),
      std::forward_as_tuple(
          std::in_place_type<comparison_match>,
          std::move(cmp),
          tagvalue));
}

void tag_matcher::check_presence(std::string&& tagname) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(tagname)),
      std::forward_as_tuple(std::in_place_type<presence_match>));
}

void tag_matcher::check_presence(std::string_view tagname) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(tagname),
      std::forward_as_tuple(std::in_place_type<presence_match>));
}

void tag_matcher::check_absence(std::string&& tagname) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(std::move(tagname)),
      std::forward_as_tuple(std::in_place_type<absence_match>));
}

void tag_matcher::check_absence(std::string_view tagname) {
  matcher_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(tagname),
      std::forward_as_tuple(std::in_place_type<absence_match>));
}


class monsoon_expr_local_ selector_accept_wrapper
: public acceptor<group_name, metric_name, metric_value>
{
 public:
  using emit_type = expression::emit_type;

  selector_accept_wrapper(acceptor<emit_type>& accept) noexcept
  : accept_(accept)
  {}

  ~selector_accept_wrapper() noexcept override = default;

  void accept_speculative(time_point, const group_name&, const metric_name&,
      const metric_value&) override;
  void accept(time_point,
      acceptor<group_name, metric_name, metric_value>::vector_type) override;

 private:
  acceptor<emit_type>& accept_;
};


class monsoon_expr_local_ selector_with_tags
: public expression
{
 public:
  selector_with_tags(path_matcher&& g, tag_matcher&& t, path_matcher&& m)
  : group_(std::move(g)),
    tags_(std::move(t)),
    metric_(std::move(m))
  {}

  ~selector_with_tags() noexcept override;

  void operator()(acceptor<emit_type>&, const metric_source&,
      const time_range&, time_point::duration) const override;

 private:
  void do_ostream(std::ostream&) const override;

  path_matcher group_;
  tag_matcher tags_;
  path_matcher metric_;
};

class monsoon_expr_local_ selector_without_tags
: public expression
{
 public:
  selector_without_tags(path_matcher&& g, path_matcher&& m)
  : group_(std::move(g)),
    metric_(std::move(m))
  {}

  ~selector_without_tags() noexcept override;

  void operator()(acceptor<emit_type>&, const metric_source&,
      const time_range&, time_point::duration) const override;

 private:
  void do_ostream(std::ostream&) const override;

  path_matcher group_;
  path_matcher metric_;
};


auto selector(path_matcher g, path_matcher m) -> expression_ptr {
  return expression::make_ptr<selector_without_tags>(
      std::move(g), std::move(m));
}

auto selector(path_matcher g, tag_matcher t, path_matcher m)
-> expression_ptr {
  return expression::make_ptr<selector_with_tags>(
      std::move(g), std::move(t), std::move(m));
}

auto selector(path_matcher g, std::optional<tag_matcher> t, path_matcher m)
-> expression_ptr {
  return (t.has_value()
      ? selector(std::move(g), std::move(t).value(), std::move(m))
      : selector(std::move(g), std::move(m)));
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


selector_with_tags::~selector_with_tags() noexcept {}

void selector_with_tags::operator()(acceptor<emit_type>& accept,
    const metric_source& source,
    const time_range& tr,
    time_point::duration slack) const {
  selector_accept_wrapper wrapped_accept{ accept };

  auto filter = source.tagged_metrics(tr);
  auto iter = filter.cbegin();
  while (iter != filter.cend()) {
    if (group_(std::get<0>(*iter).get_path())
        && tags_(std::get<0>(*iter).get_tags())
        && metric_(std::get<1>(*iter)))
      ++iter;
    else
      iter = filter.erase(iter);
  }

  source.emit(wrapped_accept, tr, std::move(filter), slack);
}

void selector_with_tags::do_ostream(std::ostream& out) const {
  out << group_ << tags_ << "::" << metric_;
}


selector_without_tags::~selector_without_tags() noexcept {}

void selector_without_tags::operator()(acceptor<emit_type>& accept,
    const metric_source& source,
    const time_range& tr,
    time_point::duration slack) const {
  selector_accept_wrapper wrapped_accept{ accept };

  auto filter = source.untagged_metrics(tr);
  auto iter = filter.cbegin();
  while (iter != filter.cend()) {
    if (group_(std::get<0>(*iter))
        && metric_(std::get<1>(*iter)))
      ++iter;
    else
      iter = filter.erase(iter);
  }

  source.emit(wrapped_accept, tr, std::move(filter), slack);
}

void selector_without_tags::do_ostream(std::ostream& out) const {
  out << group_ << "::" << metric_;
}


void selector_accept_wrapper::accept_speculative(time_point tp,
    const group_name& group, const metric_name& metric,
    const metric_value& value) {
  accept_.accept_speculative(tp,
      emit_type(std::make_tuple(group.get_tags(), value)));
}

void selector_accept_wrapper::accept(time_point tp,
    acceptor<group_name, metric_name, metric_value>::vector_type values) {
  std::vector<std::tuple<emit_type>> converted_values;
  std::for_each(
      std::make_move_iterator(values.begin()),
      std::make_move_iterator(values.end()),
      [&converted_values](auto&& v) {
        converted_values.emplace_back(
            std::make_tuple(
                std::get<0>(std::move(v)).get_tags(),
                std::get<2>(std::move(v))));
      });

  accept_.accept(tp, std::move(converted_values));
}


}} /* namespace monsoon::expressions */
