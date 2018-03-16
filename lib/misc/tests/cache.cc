#include <monsoon/cache/cache.h>
#include <monsoon/cache/impl.h>
#include <chrono>
#include <memory>
#include <vector>
#include <thread>
#include "UnitTest++/UnitTest++.h"

using namespace monsoon::cache;

struct mock_int_to_int_fn {
  template<typename Alloc>
  auto operator()([[maybe_unused]] Alloc alloc, int i) const
  noexcept
  -> int {
    return 2 * i;
  }
};

// HACKS
namespace std {

template<typename T, typename A>
auto operator<<(std::ostream& o, const std::vector<T, A>& v)
-> std::ostream& {
  bool first = true;
  for (const auto& e : v)
    o << (std::exchange(first, false) ? "[ " : ", ") << e;
  return o << (std::exchange(first, false) ? "[]" : " ]");
}

}

TEST(base_case) {
  std::vector<int> invocations;

  cache<int, int> c = cache<int, int>::builder()
      .not_thread_safe()
      .build(
          [&invocations]([[maybe_unused]] const auto& alloc, int i) {
            invocations.push_back(i);
            return 2 * i;
          });

  auto ptr = c(4);
  REQUIRE CHECK(ptr != nullptr);
  CHECK_EQUAL(8, *ptr);
  CHECK_EQUAL(std::vector<int>({ 4 }), invocations);

  // Second read should hit same value, because of ptr being live.
  auto second_ptr = c(4);
  CHECK_EQUAL(ptr, second_ptr);
  // Should not have invoked functor.
  CHECK_EQUAL(std::vector<int>({ 4 }), invocations);

  // Drop pointers, which should make cache release them.
  ptr.reset();
  second_ptr.reset();

  // Another read, should now trip the build functor again.
  auto third_ptr = c(4);
  REQUIRE CHECK(third_ptr != nullptr);
  CHECK_EQUAL(8, *third_ptr);
  CHECK_EQUAL(std::vector<int>({ 4, 4 }), invocations);
}

TEST(cache_size) {
  cache<int, int> c = cache<int, int>::builder()
      .not_thread_safe()
      .max_size(4)
      .build(mock_int_to_int_fn());

  c(1);
  c(2);
  c(3);
  c(4);
  c(5); // Expire '1' due to cache size limitation.

  CHECK_EQUAL(std::shared_ptr<int>(nullptr), c.get_if_present(1));
  CHECK(c.get_if_present(2) != nullptr);
  CHECK(c.get_if_present(3) != nullptr);
  CHECK(c.get_if_present(4) != nullptr);
  CHECK(c.get_if_present(5) != nullptr);
}

TEST(cache_memory) {
  cache<int, int> c = cache<int, int>::builder()
      .with_allocator(cache_allocator<std::allocator<int>>())
      .not_thread_safe()
      .max_memory(500 * sizeof(int))
      .build(mock_int_to_int_fn());

  for (int i = 0; i < 1000; ++i)
    c(i);

  CHECK(c.get_if_present(999) != nullptr);
  CHECK(c.get_if_present(0) == nullptr);
}

TEST(cache_max_age) {
  cache<int, int> c = cache<int, int>::builder()
      .not_thread_safe()
      .max_age(std::chrono::seconds(1))
      .build(mock_int_to_int_fn());

  auto ptr = c(4);
  REQUIRE CHECK_EQUAL(ptr, c(4));

  std::this_thread::sleep_for(std::chrono::seconds(2));
  CHECK(ptr != c(4)); // Reload must be performed.
}

int main() {
  return UnitTest::RunAllTests();
}
