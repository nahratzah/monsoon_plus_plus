#ifndef MONSOON_TAG_MATCHER_INL_H
#define MONSOON_TAG_MATCHER_INL_H

namespace monsoon {


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


} /* namespace monsoon */

#endif /* MONSOON_TAG_MATCHER_INL_H */
