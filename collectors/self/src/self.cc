#include <monsoon/collectors/self.h>
#include <monsoon/simple_group.h>
#include <monsoon/tags.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/instrumentation.h>
#include <instrumentation/visitor.h>
#include <instrumentation/counter.h>
#include <instrumentation/gauge.h>
#include <instrumentation/timing.h>
#include <instrumentation/timing_accumulate.h>
#include <chrono>
#include <utility>
#include <vector>

namespace monsoon::collectors {
namespace {


using namespace ::instrumentation;


class instr_visitor final
: public visitor
{
 public:
  instr_visitor(
      const group& root_group,
      std::vector<collector::collection_element>& elements) noexcept
  : root_path_(root_group.name()),
    elements_(elements)
  {}

  ~instr_visitor() noexcept override;

  auto operator()(const counter& c) -> void override;
  auto operator()(const gauge<bool>& g) -> void override;
  auto operator()(const gauge<std::int64_t>& g) -> void override;
  auto operator()(const gauge<double>& g) -> void override;
  auto operator()(const gauge<std::string>& g) -> void override;
  auto operator()(const timing& t) -> void override;
  auto operator()(const timing_accumulate& t) -> void override;

 private:
  auto make_group_name_(const basic_metric& m) const -> group_name;
  auto make_metric_name_(const basic_metric& m) const -> metric_name;

  std::vector<std::string_view> root_path_;
  std::vector<collector::collection_element>& elements_;
};

auto collect_self(const group& root_group, const time_point& tp)
-> collector::collection {
  std::vector<collector::collection_element> elements;
  {
    instr_visitor v{root_group, elements};
    root_group.visit(v);
  }

  return { tp, std::move(elements), true };
}

instr_visitor::~instr_visitor() noexcept {}

auto instr_visitor::operator()(const counter& c) -> void {
  elements_.emplace_back(
      make_group_name_(c),
      make_metric_name_(c),
      metric_value(*c));
}

auto instr_visitor::operator()(const gauge<bool>& g) -> void {
  elements_.emplace_back(
      make_group_name_(g),
      make_metric_name_(g),
      metric_value(*g));
}

auto instr_visitor::operator()(const gauge<std::int64_t>& g) -> void {
  elements_.emplace_back(
      make_group_name_(g),
      make_metric_name_(g),
      metric_value(*g));
}

auto instr_visitor::operator()(const gauge<double>& g) -> void {
  elements_.emplace_back(
      make_group_name_(g),
      make_metric_name_(g),
      metric_value(*g));
}

auto instr_visitor::operator()(const gauge<std::string>& g) -> void {
  elements_.emplace_back(
      make_group_name_(g),
      make_metric_name_(g),
      metric_value(*g));
}

auto instr_visitor::operator()(const timing& t) -> void {
  using tdelta = std::chrono::duration<double, std::milli>;

  histogram h;
  for (const auto& t_item : t) {
    h.add(
        { tdelta(t_item.lo).count(), tdelta(t_item.hi).count() },
        t_item.count);
  }

  elements_.emplace_back(
      make_group_name_(t),
      make_metric_name_(t),
      metric_value(std::move(h)));
}

auto instr_visitor::operator()(const timing_accumulate& t) -> void {
  using tdelta = std::chrono::duration<double, std::milli>;

  elements_.emplace_back(
      make_group_name_(t),
      make_metric_name_(t),
      metric_value(tdelta(*t).count()));
}

auto instr_visitor::make_group_name_(const basic_metric& m) const
-> group_name {
  std::vector<std::pair<tag_map::key_type, metric_value>> tagset;
  std::transform(m.tags.begin(), m.tags.end(), std::back_inserter(tagset),
      [](const auto& instr_tag_elem) {
        return std::make_pair(
            instr_tag_elem.first,
            std::visit(
                [](const auto& x) {
                  return metric_value(x);
                },
                instr_tag_elem.second));
      });
  return group_name(simple_group(root_path_), tags(std::move(tagset)));
}

auto instr_visitor::make_metric_name_(const basic_metric& m) const
-> metric_name {
  if (m.name.size() < root_path_.size())
    throw std::logic_error("Metric has shorter name than its parent group.");

  return metric_name(m.name.begin() + root_path_.size(), m.name.end());
}


} /* namespace monsoon::collectors::<unnamed> */


self::self() noexcept
: self(monsoon_instrumentation())
{}

self::self(const instrumentation::group& grp) noexcept
: grp_(grp)
{}

self::~self() noexcept {}

auto self::provides() const
-> names_set {
  path_matcher root_path;
  for (std::string_view name_elem : grp_.name())
    root_path.push_back_literal(name_elem);

  return {
    {}, // No known names.
    { // Wildcarded root_path::**
      { std::move(root_path),
        tag_matcher(),
        path_matcher().push_back_double_wildcard() }
    }
  };
}

auto self::run(objpipe::reader<time_point> tp_pipe) const
-> objpipe::reader<collection> {
  const instrumentation::group* grp = &grp_;

  return std::move(tp_pipe)
      .transform(
          [grp](const time_point& tp) {
            return collect_self(*grp, tp);
          });
}


} /* namespace monsoon::collectors */
