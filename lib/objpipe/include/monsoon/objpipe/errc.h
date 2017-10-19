#ifndef MONSOON_OBJPIPE_ERRC_H
#define MONSOON_OBJPIPE_ERRC_H

#include <monsoon/objpipe/pipe_export_.h>
#include <string>
#include <system_error>
#include <type_traits>

namespace monsoon {
namespace objpipe {


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


class monsoon_pipe_export_ objpipe_category_t
: public std::error_category
{
 public:
  const char* name() const noexcept override;
  std::error_condition default_error_condition(int) const noexcept override;
  bool equivalent(const std::error_code&, int) const noexcept override;
  std::string message(int) const override;
};

monsoon_pipe_export_
const objpipe_category_t& objpipe_category();

monsoon_pipe_export_
std::error_condition make_error_condition(objpipe_errc);


}} /* namespace monsoon::objpipe */

#endif /* MONSOON_OBJPIPE_ERRC_H */
