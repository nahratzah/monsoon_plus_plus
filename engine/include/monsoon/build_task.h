#ifndef MONSOON_BUILD_TASK_H
#define MONSOON_BUILD_TASK_H

#include <monsoon/configuration.h>
#include <monsoon/time_point.h>
#include <monsoon/history/collect_history.h>
#include <objpipe/interlock.h>
#include <memory>
#include <vector>

namespace monsoon {


auto build_task(
    const configuration& cfg,
    const std::vector<std::shared_ptr<collect_history>>& histories)
-> objpipe::interlock_writer<time_point>;


} /* namespace monsoon */

#endif /* MONSOON_BUILD_TASK_H */
