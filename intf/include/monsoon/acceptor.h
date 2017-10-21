#ifndef MONSOON_ACCEPTOR_H
#define MONSOON_ACCEPTOR_H

///\file
///\ingroup intf
///\deprecated This interface was a mistake and it should go away.

#include <type_traits>
#include <tuple>
#include <vector>
#include <monsoon/time_point.h>

namespace monsoon {


///\brief Metric acceptor type.
///\ingroup intf
///\deprecated This interface was a mistake and it should go away.
template<typename... Types>
class acceptor {
 public:
  using tuple_type =
      std::tuple<std::remove_cv_t<std::remove_reference_t<Types>>...>;
  using vector_type = std::vector<tuple_type>;

  virtual ~acceptor() noexcept;
  virtual void accept_speculative(time_point,
      std::add_lvalue_reference_t<std::add_const_t<Types>>...);
  virtual void accept(time_point, vector_type) = 0;
};


} /* namespace monsoon */

#include "acceptor-inl.h"

#endif /* MONSOON_ACCEPTOR_H */
