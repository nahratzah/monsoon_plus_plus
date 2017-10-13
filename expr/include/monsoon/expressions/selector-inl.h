#ifndef MONSOON_EXPRESSIONS_SELECTOR_INL_H
#define MONSOON_EXPRESSIONS_SELECTOR_INL_H

namespace monsoon {
namespace expressions {


inline path_matcher::path_matcher(path_matcher&& o) noexcept
: matcher_(std::move(o.matcher_))
{}

inline auto path_matcher::operator=(path_matcher&& o) noexcept
-> path_matcher& {
  matcher_ = std::move(o.matcher_);
  return *this;
}


}} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_SELECTOR_INL_H */
