#ifndef MONSOON_CONFIGURATION_H
#define MONSOON_CONFIGURATION_H

#include <monsoon/collector.h>
#include <memory>
#include <vector>

namespace monsoon {


class configuration {
 public:
  configuration() = default;
  configuration(const configuration&) = delete;
  configuration(configuration&&) noexcept;
  configuration& operator=(const configuration&) = delete;
  configuration& operator=(configuration&&) noexcept;

  bool empty() const noexcept;
  const std::vector<std::unique_ptr<collector>>& collectors() const noexcept;
  const std::vector<std::unique_ptr<rule>>& rules() const noexcept;

 private:
  std::vector<std::unique_ptr<collector>> collectors_;
  std::vector<std::unique_ptr<rule>> rules_;
};


} /* namespace monsoon */

#include "configuration-inl.h"

#endif /* MONSOON_CONFIGURATION_H */
