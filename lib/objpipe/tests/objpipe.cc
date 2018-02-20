#include <monsoon/objpipe/callback.h>
#include <monsoon/objpipe/array.h>
#include <monsoon/objpipe/interlock.h>
#include <monsoon/objpipe/errc.h>
#include "UnitTest++/UnitTest++.h"
#include "test_hacks.ii"
#include <vector>
#include <tuple>
#include <thread>

using monsoon::objpipe::objpipe_errc;

TEST(callback) {
  auto reader = monsoon::objpipe::new_callback<int>(
      [](auto& cb) {
        for (int i = 0; i < 5; ++i)
          cb(i);
      });

  CHECK_EQUAL(false, reader.empty());

  // Element 0 access, using front() and pop_front().
  CHECK_EQUAL(0, reader.front());
  reader.pop_front();

  // Element 1 access, using front() and pull().
  CHECK_EQUAL(1, reader.front());
  CHECK_EQUAL(1, reader.pull());

  // Element 2 access, using pull().
  CHECK_EQUAL(2, reader.pull());

  // Element 3 access, using try_pull().
  CHECK_EQUAL(3, reader.try_pull());

  // Element 4 access, using wait(), then pull().
  CHECK_EQUAL(objpipe_errc::success, reader.wait());
  CHECK_EQUAL(4, reader.pull());

  // No more elements.
  CHECK_EQUAL(false, reader.is_pullable());
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);
}

TEST(array_using_iterators) {
  std::vector<int> il = { 0, 1, 2, 3, 4 };
  auto reader = monsoon::objpipe::new_array(il.begin(), il.end());

  CHECK_EQUAL(false, reader.empty());

  // Element 0 access, using front() and pop_front().
  CHECK_EQUAL(0, reader.front());
  reader.pop_front();

  // Element 1 access, using front() and pull().
  CHECK_EQUAL(1, reader.front());
  CHECK_EQUAL(1, reader.pull());

  // Element 2 access, using pull().
  CHECK_EQUAL(2, reader.pull());

  // Element 3 access, using try_pull().
  CHECK_EQUAL(3, reader.try_pull());

  // Element 4 access, using wait(), then pull().
  CHECK_EQUAL(objpipe_errc::success, reader.wait());
  CHECK_EQUAL(4, reader.pull());

  // No more elements.
  CHECK_EQUAL(false, reader.is_pullable());
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);
}

TEST(array_using_initializer_list) {
  auto reader = monsoon::objpipe::new_array({ 0, 1, 2, 3, 4 });

  CHECK_EQUAL(false, reader.empty());

  // Element 0 access, using front() and pop_front().
  CHECK_EQUAL(0, reader.front());
  reader.pop_front();

  // Element 1 access, using front() and pull().
  CHECK_EQUAL(1, reader.front());
  CHECK_EQUAL(1, reader.pull());

  // Element 2 access, using pull().
  CHECK_EQUAL(2, reader.pull());

  // Element 3 access, using try_pull().
  CHECK_EQUAL(3, reader.try_pull());

  // Element 4 access, using wait(), then pull().
  CHECK_EQUAL(objpipe_errc::success, reader.wait());
  CHECK_EQUAL(4, reader.pull());

  // No more elements.
  CHECK_EQUAL(false, reader.is_pullable());
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);
}

TEST(filter_operation) {
  auto reader = monsoon::objpipe::new_array({ 0, 1, 2, 3, 4 })
      .filter([](int x) { return x % 2 == 0; });

  CHECK_EQUAL(false, reader.empty());

  // Element 0 access, using front() and pop_front().
  CHECK_EQUAL(0, reader.front());
  reader.pop_front();

  // Element 1 is filtered out

  // Element 2 access, using pull().
  CHECK_EQUAL(2, reader.pull());

  // Element 3 is filtered out

  // Element 4 access, using wait(), then pull().
  CHECK_EQUAL(objpipe_errc::success, reader.wait());
  CHECK_EQUAL(4, reader.pull());

  // No more elements.
  CHECK_EQUAL(false, reader.is_pullable());
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);
}

TEST(transform_operation) {
  auto reader = monsoon::objpipe::new_array({ 0, 1, 2, 3, 4 })
      .transform([](int x) { return 2 * x; });

  CHECK_EQUAL(false, reader.empty());

  // Element 0 access, using front() and pop_front().
  CHECK_EQUAL(0, reader.front());
  reader.pop_front();

  // Element 1 access, using front() and pull().
  CHECK_EQUAL(2, reader.front());
  CHECK_EQUAL(2, reader.pull());

  // Element 2 access, using pull().
  CHECK_EQUAL(4, reader.pull());

  // Element 3 access, using try_pull().
  CHECK_EQUAL(6, reader.try_pull());

  // Element 4 access, using wait(), then pull().
  CHECK_EQUAL(objpipe_errc::success, reader.wait());
  CHECK_EQUAL(8, reader.pull());

  // No more elements.
  CHECK_EQUAL(false, reader.is_pullable());
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);
}

TEST(interlock) {
  monsoon::objpipe::interlock_reader<int> reader;
  monsoon::objpipe::interlock_writer<int> writer;
  std::tie(reader, writer) = monsoon::objpipe::new_interlock<int>();

  auto th = std::thread(std::bind(
          [](monsoon::objpipe::interlock_writer<int>& writer) {
            for (int i = 0; i < 5; ++i)
              writer(i);
          },
          std::move(writer)));

  CHECK_EQUAL(objpipe_errc::success, reader.wait());
  CHECK_EQUAL(false, reader.empty());

  // Element 0 access, using front() and pop_front().
  CHECK_EQUAL(0, reader.front());
  reader.pop_front();

  // Element 1 access, using front() and pull().
  CHECK_EQUAL(1, reader.front());
  CHECK_EQUAL(1, reader.pull());

  // Element 2 access, using pull().
  CHECK_EQUAL(2, reader.pull());

  // Element 3 access, using try_pull().
  // Note: try_pull may yield empty optional, if the writer thread is not fast
  // enough. So we spin on that.
  std::optional<int> three = reader.try_pull();
  while (!three.has_value()) three = reader.try_pull();
  CHECK_EQUAL(3, three);

  // Element 4 access, using wait(), then pull().
  CHECK_EQUAL(objpipe_errc::success, reader.wait());
  CHECK_EQUAL(4, reader.pull());

  // No more elements.
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());
  CHECK_EQUAL(false, reader.is_pullable());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);

  th.join();
}

TEST(reader_to_vector) {
  CHECK_EQUAL(
      std::vector<int>({ 0, 1, 2, 3, 4 }),
      monsoon::objpipe::new_array({ 0, 1, 2, 3, 4 })
          .to_vector());
}

int main() {
  return UnitTest::RunAllTests();
}
