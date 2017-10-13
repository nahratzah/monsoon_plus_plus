#include <monsoon/expressions/selector.h>
#include "../overload.h"
#include <ostream>
#include <iterator>

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
