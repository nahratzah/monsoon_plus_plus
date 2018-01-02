#ifndef MONSOON_PATH_MATCHER_INL_H
#define MONSOON_PATH_MATCHER_INL_H

namespace monsoon {


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


} /* namespace monsoon */

#endif /* MONSOON_PATH_MATCHER_INL_H */
