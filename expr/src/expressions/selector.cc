#include <monsoon/expressions/selector.h>
#include <monsoon/overload.h>
#include <ostream>
#include <iterator>
#include <sstream>
#include <monsoon/config_support.h>

namespace monsoon {
namespace expressions {


class monsoon_expr_local_ selector_with_tags
: public expression
{
 public:
  selector_with_tags(path_matcher&& g, tag_matcher&& t, path_matcher&& m)
  : expression(precedence_value),
    group_(std::move(g)),
    tags_(std::move(t)),
    metric_(std::move(m))
  {}

  ~selector_with_tags() noexcept override;

  auto operator()(const metric_source&, const time_range&,
      time_point::duration,
      const std::shared_ptr<const match_clause>&) const
      -> std::variant<scalar_objpipe, vector_objpipe> override;

  bool is_scalar() const noexcept override;
  bool is_vector() const noexcept override;

 private:
  void do_ostream(std::ostream&) const override;

  path_matcher group_;
  tag_matcher tags_;
  path_matcher metric_;
};


auto selector(path_matcher g, path_matcher m) -> expression_ptr {
  return selector(std::move(g), tag_matcher(), std::move(m));
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

auto selector_with_tags::operator()(
    const metric_source& source,
    const time_range& tr,
    time_point::duration slack,
    const std::shared_ptr<const match_clause>& mc) const
-> std::variant<scalar_objpipe, vector_objpipe> {
  using namespace std::placeholders;

  return source
      .emit(tr, group_, tags_, metric_, slack)
      .transform(
          [mc](const metric_source::emit_type& src) -> expression::vector_emit_type {
            using match_clause_hash = class match_clause::hash;
            using match_clause_equal_to = match_clause::equal_to;

            if (src.index() == 0) {
              const auto& speculative = std::get<0>(src);
              const time_point& tp = std::get<0>(speculative);
              const tags& tag_set = std::get<1>(speculative).get_tags();
              const metric_value& value = std::get<3>(speculative);

              return {
                  tp,
                  std::in_place_index<0>,
                  tag_set,
                  value
              };
            } else {
              const auto& factual = std::get<1>(src);
              const time_point tp = std::get<0>(factual);
              const auto& map = std::get<1>(factual);

              // Fill in the map of the factual result.
              expression::factual_vector out_map = expression::factual_vector(
                  8u,
                  match_clause_hash(mc),
                  match_clause_equal_to(mc));
              std::transform(map.begin(), map.end(),
                  std::inserter(out_map, out_map.end()),
                  [](const auto& metric) {
                    return std::forward_as_tuple(
                        std::get<0>(metric.first).get_tags(),
                        metric.second);
                  });

              return {
                  tp,
                  std::in_place_index<1>,
                  std::move(out_map)
              };
            }
          });
}

bool selector_with_tags::is_scalar() const noexcept {
  return false;
}

bool selector_with_tags::is_vector() const noexcept {
  return true;
}

void selector_with_tags::do_ostream(std::ostream& out) const {
  out << group_ << tags_ << "::" << metric_;
}


}} /* namespace monsoon::expressions */
