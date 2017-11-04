#include <monsoon/objpipe/detail/reader_intf.h>

namespace monsoon {
namespace objpipe {
namespace detail {


continuation_intf::~continuation_intf() noexcept {}

void continuation_intf::on_last_writer_gone_() noexcept {}


}}} /* namespace monsoon::objpipe::detail */
