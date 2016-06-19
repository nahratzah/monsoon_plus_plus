#ifndef MONSOON_MATCH_CLAUSE_H
#define MONSOON_MATCH_CLAUSE_H

#include <monsoon/metric_value.h>
#include <monsoon/tags.h>
#include <functional>
#include <iosfwd>
#include <string>
#include <unordered_map>

namespace monsoon {


class match_clause {
 public:
  match_clause(bool = false) noexcept;
  virtual ~match_clause() noexcept;

  virtual std::unordered_map<tags, metric_value> apply(
      std::unordered_map<tags, metric_value>,
      std::unordered_map<tags, metric_value>,
      std::function<metric_value(metric_value, metric_value)>) = 0;
  virtual void do_ostream(std::ostream&) const = 0;
  std::string config_string() const;

  const bool empty_config_string;
};


std::ostream& operator<<(std::ostream&, const match_clause&);


} /* namespace monsoon */

#endif /* MONSOON_MATCH_CLAUSE_H */
