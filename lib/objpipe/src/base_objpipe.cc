#include <monsoon/objpipe/detail/base_objpipe.h>

namespace monsoon {
namespace objpipe {
namespace detail {


base_objpipe::~base_objpipe() noexcept {}

void reader_release::operator()(base_objpipe* ptr) const noexcept {
  if (ptr->reader_refcnt_.fetch_sub(1u, std::memory_order_release) == 1u)
    ptr->on_last_reader_gone_();
  if (ptr->refcnt_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete ptr;
}

void writer_release::operator()(base_objpipe* ptr) const noexcept {
  if (ptr->writer_refcnt_.fetch_sub(1u, std::memory_order_release) == 1u)
    ptr->on_last_writer_gone_();
  if (ptr->refcnt_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete ptr;
}


}}} /* namespace monsoon::objpipe::detail */
