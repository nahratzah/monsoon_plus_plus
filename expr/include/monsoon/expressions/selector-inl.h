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

inline auto path_matcher::begin() const -> const_iterator {
  return matcher_.begin();
}

inline auto path_matcher::end() const -> const_iterator {
  return matcher_.end();
}

inline auto path_matcher::cbegin() const -> const_iterator {
  return matcher_.cbegin();
}

inline auto path_matcher::cend() const -> const_iterator {
  return matcher_.cend();
}


inline tag_matcher::tag_matcher(tag_matcher&& o) noexcept
: matcher_(std::move(o.matcher_))
{}

inline auto tag_matcher::operator=(tag_matcher&& o) noexcept
-> tag_matcher& {
  matcher_ = std::move(o.matcher_);
  return *this;
}

inline auto tag_matcher::begin() const -> const_iterator {
  return matcher_.begin();
}

inline auto tag_matcher::end() const -> const_iterator {
  return matcher_.end();
}

inline auto tag_matcher::cbegin() const -> const_iterator {
  return matcher_.cbegin();
}

inline auto tag_matcher::cend() const -> const_iterator {
  return matcher_.cend();
}


}} /* namespace monsoon::expressions */

#endif /* MONSOON_EXPRESSIONS_SELECTOR_INL_H */
