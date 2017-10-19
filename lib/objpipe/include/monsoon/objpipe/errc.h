#ifndef MONSOON_OBJPIPE_ERRC_H
#define MONSOON_OBJPIPE_ERRC_H

///@file monsoon/objpipe/errc.h <monsoon/objpipe/errc.h>

#include <monsoon/objpipe/objpipe_export_.h>
#include <string>
#include <system_error>
#include <type_traits>

namespace monsoon {
namespace objpipe {


/**
 * Object pipe error conditions.
 */
enum class objpipe_errc {
  success=0,
  closed
};


}} /* namespace monsoon::objpipe */

namespace std {


template<>
struct is_error_condition_enum<monsoon::objpipe::objpipe_errc>
: true_type
{};


} /* namespace std */

namespace monsoon {
namespace objpipe {


/**
 * Specialization of error_category, for objpipe errors.
 */
class monsoon_objpipe_export_ objpipe_category_t
: public std::error_category
{
 public:
  const char* name() const noexcept override;
  std::error_condition default_error_condition(int) const noexcept override;
  bool equivalent(const std::error_code&, int) const noexcept override;
  std::string message(int) const override;
};

/**
 * @return the object pipe error category.
 */
monsoon_objpipe_export_
const objpipe_category_t& objpipe_category();

/**
 * Create a objpipe_category error condition.
 * @param e The error code for which to create an error condition.
 */
monsoon_objpipe_export_
std::error_condition make_error_condition(objpipe_errc e);


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_ERRC_H */
