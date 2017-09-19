#include <monsoon/histogram.h>
#include <algorithm>
#include <cassert>
#include <iterator>
#include <ostream>
#include <sstream>

namespace monsoon {


auto histogram::map() const -> std::map<range, std::double_t> {
  std::map<range, std::double_t> result;
  for (const auto& e : elems_)
    result.emplace(std::get<0>(e), std::get<1>(e));
  return result;
}

auto histogram::add(range r, std::double_t c) -> histogram& {
  const auto insert_val = elems_vector::value_type(r, c, 0);
  elems_.insert(std::upper_bound(elems_.begin(), elems_.end(),
                                 insert_val),
                insert_val);
  fixup_immed_();

  return *this;
}

auto histogram::before(const histogram& x, const histogram& y) noexcept
->  bool {
  return std::lexicographical_compare(x.elems_.begin(), x.elems_.end(),
                                      y.elems_.begin(), y.elems_.end());
}

auto histogram::add_immed_(range r, std::double_t c) -> void {
  elems_.emplace_back(r, c, 0);
}

auto histogram::fixup_immed_unsorted_() -> void {
  std::sort(elems_.begin(), elems_.end());
  fixup_immed_();
}

auto histogram::fixup_immed_() -> void {
  assert(std::is_sorted(elems_.begin(), elems_.end()));

  if (elems_.size() >= 2) {
    // Stage 1: split any intersecting ranges.
    for (elems_vector::iterator i = elems_.begin();
         next(i) != elems_.end();
         /* SKIP */) {
      elems_vector::iterator j = std::next(i);
      range& i_r = std::get<0>(*i);
      range& j_r = std::get<0>(*j);
      std::double_t& i_count = std::get<1>(*i);
      std::double_t& j_count = std::get<1>(*j);

      // Loop invariant.
      assert(i_r <= j_r);

      // Skip if ranges don't intersect.
      if (i_r.high() <= j_r.low()) {
        ++i;
        continue;
      }

      // If first range has partial intersection with second range,
      // split the first range.
      if (i_r.low() < j_r.low()) {
        auto left_weight = (j_r.low() - i_r.low()) / i_r.width();
        auto right_weight = 1 - left_weight;

        // Calculate right-side of the split.
        auto new_count = i_count * right_weight;
        auto new_range = range(j_r.low(), i_r.high());
        // Update i to represent the left-side of the split.
        i_r = range(i_r.low(), j_r.low());
        i_count *= left_weight;
        // Add right-side of split, make i point at the newly inserted element.
        i = elems_.emplace(j, std::move(new_range), std::move(new_count), 0);
        continue;
      }

      // If first range is a subrange of the second range,
      // split the second range and move the left-side of the split into i.
      //
      // Note that because of ordering, both start with the same range::low().
      if (i_r.high() < j_r.high()) {
        auto left_weight = i_r.width() / j_r.width();
        auto right_weight = 1 - left_weight;

        // Add left-side count to i.
        i_count += j_count * left_weight;
        j_count *= right_weight;
        j_r = range(i_r.high(), j_r.high());

        // Move j to the correct position.
        auto new_j_successor = std::upper_bound(std::next(j), elems_.end(), *j);
        if (new_j_successor != std::next(j))
          std::rotate(j, std::next(j), new_j_successor);
        else  // No re-ordering, advance iterator.
          ++i;
        continue;
      }
    }

    // Stage 2: combine ranges that are equal.
    for (elems_vector::iterator i = elems_.begin();
         next(i) != elems_.end();
         ++i) {
      elems_vector::iterator j = std::next(i);
      range& i_r = std::get<0>(*i);
      range& j_r = std::get<0>(*j);
      std::double_t& i_count = std::get<1>(*i);
      std::double_t& j_count = std::get<1>(*j);

      // Loop invariant.
      assert(i_r <= j_r);

      // Move count from first into second, so that if multiple equal
      // ranges follow eachother, they are all combined.
      // The first range is left with a zero count, that will be filtered
      // out in stage 3.
      if (i_r == j_r)
        j_count += std::exchange(i_count, 0);
    }
  }

  // Stage 3: remove empty ranges.
  fixup_immed_erase_empty_();

  // Verify invariant.
  assert(std::is_sorted(elems_.begin(), elems_.end()));

  // Update the running count.
  std::double_t running_count = 0;
  for (auto& e : elems_)
    std::get<2>(e) = (running_count += std::get<1>(e));

  // Reduce memory usage.
  elems_.shrink_to_fit();
}

auto histogram::fixup_immed_erase_empty_() -> void {
  elems_.erase(std::remove_if(elems_.begin(), elems_.end(),
                              [](elems_vector::const_reference r) {
                                return std::get<0>(r).width() == 0.0 ||
                                       std::get<0>(r).width() == -0.0;
                              }),
               elems_.end());
}


auto operator-(histogram x) noexcept -> histogram {
  for (auto& e : x.elems_) {
    std::get<1>(e) = -std::get<1>(e);
    std::get<2>(e) = -std::get<2>(e);
  }
  return x;
}

auto operator+(histogram&& x, const histogram& y) -> histogram {
  x.elems_.insert(x.elems_.end(), y.elems_.begin(), y.elems_.end());
  x.fixup_immed_unsorted_();
  return x;
}

auto operator+(const histogram& x, histogram&& y) -> histogram {
  return std::move(y) + x;
}

auto operator+(histogram&& x, histogram&& y) -> histogram {
  x.elems_.insert(x.elems_.end(), y.elems_.begin(), y.elems_.end());
  x.fixup_immed_unsorted_();
  return x;
}

auto operator+(const histogram& x, const histogram& y) -> histogram {
  histogram copy = x;
  return std::move(copy) + y;
}

auto operator-(histogram&& x, histogram&& y) -> histogram {
  for (const auto& e : y.elems_)
    x.add_immed_(std::get<0>(e), -std::get<1>(e));
  x.fixup_immed_unsorted_();
  return x;
}

auto operator-(const histogram& x, histogram&& y) -> histogram {
  for (auto& e : y.elems_)
    std::get<1>(e) = -std::get<1>(e);
  y.elems_.insert(y.elems_.end(), x.elems_.begin(), x.elems_.end());
  y.fixup_immed_unsorted_();
  return y;
}

auto operator-(histogram&& x, const histogram& y) -> histogram {
  for (const auto& e : y.elems_)
    x.add_immed_(std::get<0>(e), -std::get<1>(e));
  x.fixup_immed_unsorted_();
  return x;
}

auto operator-(const histogram& x, const histogram& y) -> histogram {
  histogram copy = y;
  return x - std::move(copy);
}

auto operator*(histogram h, std::double_t v) noexcept -> histogram {
  for (auto& e : h.elems_) {
    std::get<1>(e) *= v;
    std::get<2>(e) *= v;  // Running count scales in the same way.
  }
  h.fixup_immed_erase_empty_();
  return h;
}

auto operator/(histogram h, std::double_t v) -> histogram {
  if (v == 0.0 || v == -0.0)
    throw std::invalid_argument("division by zero");

  for (auto& e : h.elems_) {
    std::get<1>(e) /= v;
    std::get<2>(e) /= v;  // Running count scales in the same way.
  }
  h.fixup_immed_erase_empty_();
  return h;
}

auto operator*(std::double_t v, histogram h) noexcept -> histogram {
  for (auto& e : h.elems_) {
    std::get<1>(e) *= v;
    std::get<2>(e) *= v;  // Running count scales in the same way.
  }
  h.fixup_immed_erase_empty_();
  return h;
}

auto operator-=(histogram& x, const histogram& y) -> histogram& {
  for (const auto& e : y.elems_)
    x.add_immed_(std::get<0>(e), -std::get<1>(e));
  x.fixup_immed_unsorted_();
  return x;
}

auto operator+=(histogram& x, const histogram& y) -> histogram& {
  x.elems_.insert(x.elems_.end(), y.elems_.begin(), y.elems_.end());
  x.fixup_immed_unsorted_();
  return x;
}

auto operator*=(histogram& h, std::double_t v) -> histogram& {
  for (auto& e : h.elems_) {
    std::get<1>(e) *= v;
    std::get<2>(e) *= v;  // Running count scales in the same way.
  }
  h.fixup_immed_erase_empty_();
  return h;
}

auto operator/=(histogram& h, std::double_t v) -> histogram& {
  if (v == 0.0 || v == -0.0)
    throw std::invalid_argument("division by zero");

  for (auto& e : h.elems_) {
    std::get<1>(e) /= v;
    std::get<2>(e) /= v;  // Running count scales in the same way.
  }
  h.fixup_immed_erase_empty_();
  return h;
}

auto operator<<(std::ostream& out, const histogram& h) -> std::ostream& {
  if (h.empty()) return out << "[]";

  out << "[ ";
  bool first = true;
  for (const auto& e : h.data()) {
    if (!std::exchange(first, false))
      out << ", ";

    out << std::get<0>(e).low()
        << ".."
        << std::get<0>(e).high()
        << "="
        << std::get<1>(e);
  }
  out << " ]";
  return out;
}


} /* namespace monsoon */


namespace std {


auto hash<monsoon::histogram>::operator()(const monsoon::histogram& h)
    const noexcept
->  size_t {
  size_t result = 0;
  for (const auto& e : h.data()) {
    result = 23 * result +
             std::hash<monsoon::histogram::range>()(std::get<0>(e));
    result = 29 * result + std::hash<std::double_t>()(std::get<1>(e));
  }
  return result;
}

auto to_string(const monsoon::histogram& h) -> std::string {
  return (std::ostringstream() << h).str();
}


} /* namespace std */
