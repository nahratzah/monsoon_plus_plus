#include <monsoon/objpipe/callback.h>
#include <monsoon/objpipe/array.h>
#include <monsoon/objpipe/errc.h>
#include "UnitTest++/UnitTest++.h"
#include "test_hacks.h"
#include <vector>

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
  CHECK_EQUAL(false, bool(reader));
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);
}

TEST(array_using_iterators) {
  std::vector<int> il = { 0, 1, 2, 3, 4 };
  auto reader = monsoon::objpipe::new_array<int>(il.begin(), il.end());

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
  CHECK_EQUAL(false, bool(reader));
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);
}

TEST(array_using_initializer_list) {
  auto reader = monsoon::objpipe::new_array<int>({ 0, 1, 2, 3, 4 });

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
  CHECK_EQUAL(false, bool(reader));
  CHECK_EQUAL(true, reader.empty());
  CHECK_EQUAL(objpipe_errc::closed, reader.wait());

  objpipe_errc e;
  std::optional<int> failed_pull = reader.pull(e);
  CHECK_EQUAL(false, failed_pull.has_value());
  CHECK_EQUAL(objpipe_errc::closed, e);
}

int main() {
  return UnitTest::RunAllTests();
}
