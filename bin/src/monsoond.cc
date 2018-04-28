#include <monsoon/configuration.h>
#include <monsoon/build_task.h>
#include <monsoon/time_point.h>
#include <monsoon/history/collect_history.h>
#include <monsoon/history/print_history.h>
#include <monsoon/collectors/self.h>
#include <functional>
#include <thread>
#include <memory>

using namespace monsoon;

int main(int argc, char*argv[]) {
  std::function<void(time_point)> task;

  {
    configuration cfg;
    cfg.add(std::make_unique<collectors::self>());

    std::vector<std::shared_ptr<collect_history>> histories;
    histories.push_back(std::make_shared<print_history>());
    task = build_task(cfg, histories);
  }

  for (;;) {
    task(time_point::now());
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}
