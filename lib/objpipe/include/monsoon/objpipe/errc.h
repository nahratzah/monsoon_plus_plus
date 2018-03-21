#ifndef MONSOON_OBJPIPE_ERRC_H
#define MONSOON_OBJPIPE_ERRC_H

///\file
///\ingroup objpipe objpipe_errors

#include <monsoon/objpipe/objpipe_export_.h>
#include <string>
#include <system_error>
#include <type_traits>
#include <iosfwd>

namespace monsoon {
namespace objpipe {


/**
 * \brief Object pipe error conditions.
 * \ingroup objpipe_errors
 */
enum class objpipe_errc {
  success=0, ///< Status code indicating successful completion.
  closed ///< Status code indicating failure, due to a closed object pipe.
};

/**
 * \brief Reference to the \ref objpipe_category().
 * \ingroup objpipe_errors
 * \return the object pipe error category.
 */
monsoon_objpipe_export_
const std::error_category& objpipe_category();

/**
 * \brief Create an \ref objpipe_category() error condition.
 * \ingroup objpipe_errors
 * \param e The error code for which to create an error condition.
 */
monsoon_objpipe_export_
std::error_condition make_error_condition(objpipe_errc e);

/**
 * \brief Write errc to output stream.
 * \ingroup objpipe_errors
 */
monsoon_objpipe_export_
std::ostream& operator<<(std::ostream& out, objpipe_errc e);

/**
 * \brief Objpipe exception class.
 */
class objpipe_error
: public std::system_error
{
 public:
  objpipe_error(objpipe_errc e)
  : std::system_error(static_cast<int>(e), objpipe_category())
  {}

  objpipe_error(objpipe_errc e, const std::string& what_arg)
  : std::system_error(static_cast<int>(e), objpipe_category(), what_arg)
  {}

  objpipe_error(objpipe_errc e, const char* what_arg)
  : std::system_error(static_cast<int>(e), objpipe_category(), what_arg)
  {}

  ~objpipe_error() override {};
};


}} /* namespace monsoon::objpipe */

namespace std {


template<>
struct is_error_condition_enum<monsoon::objpipe::objpipe_errc>
: true_type
{};


} /* namespace std */

#endif /* MONSOON_OBJPIPE_ERRC_H */
