#include "tsdata_xdr.h"
#include "xdr_primitives.h"
#include "record_array.h"
#include "tsdata.h"
#include <stack>
#include <tuple>

namespace monsoon::history::v2 {


auto decode(const cache_search_type<dictionary, tsdata_xdr>& cst, dictionary::allocator_type alloc)
-> std::shared_ptr<dictionary> {
  auto result = std::allocate_shared<dictionary>(alloc, alloc);
  const encdec_ctx& ctx = cst.parent()->get_ctx();

  // Build stack of segments.
  std::stack<file_segment_ptr> ptrs;
  for (std::shared_ptr<const tsdata_xdr> tsd = cst.parent();
      tsd != nullptr;
      tsd = tsd->get_predecessor()) {
    if (tsd->dict_ != file_segment_ptr())
      ptrs.push(tsd->dict_);
  }

  // Decode update for each segment.
  while (!ptrs.empty()) {
    auto xdr = ctx.new_reader(ptrs.top(), dictionary::is_compressed);
    ptrs.pop();
    result->decode_update(xdr);
    if (!xdr.at_end()) throw dirhistory_exception("xdr data remaining");
    xdr.close();
  }

  return result;
}


tsdata_xdr::tsdata_xdr(std::shared_ptr<tsdata_v2> parent, allocator_type alloc)
: typed_dynamics<dynamics>(parent),
  ctx_(parent->get_ctx())
{}

tsdata_xdr::tsdata_xdr(std::shared_ptr<tsdata_xdr> parent, allocator_type alloc)
: typed_dynamics<dynamics>(parent),
  ctx_(parent->get_ctx())
{}

tsdata_xdr::~tsdata_xdr() noexcept {}

auto tsdata_xdr::get_dictionary() const
-> std::shared_ptr<const dictionary> {
  tsdata_xdr*const parent = dynamic_cast<tsdata_xdr*>(&this->parent());
  if (parent != nullptr) return parent->get_dictionary();

  return get_dynamics_cache<dictionary>(shared_from_this(), dict_);
}

auto tsdata_xdr::get_dictionary()
-> std::shared_ptr<dictionary> {
  tsdata_xdr*const parent = dynamic_cast<tsdata_xdr*>(&this->parent());
  if (parent != nullptr) return parent->get_dictionary();

  return get_dynamics_cache<dictionary>(shared_from_this(), dict_);
}

auto tsdata_xdr::get_predecessor() const
-> std::shared_ptr<const tsdata_xdr> {
  return const_cast<tsdata_xdr&>(*this).get_predecessor();
}

auto tsdata_xdr::get() const
-> std::shared_ptr<const record_array> {
  return get_dynamics_cache<record_array>(shared_from_this(), records_);
}

auto tsdata_xdr::get_predecessor()
-> std::shared_ptr<tsdata_xdr> {
  if (pred_ == file_segment_ptr()) return nullptr;
  return get_dynamics_cache<tsdata_xdr>(std::const_pointer_cast<tsdata_xdr>(shared_from_this()), pred_);
}

auto tsdata_xdr::decode(xdr::xdr_istream& in)
-> void {
  ts_ = decode_timestamp(in);
  pred_ = in.get_optional(&file_segment_ptr::from_xdr).value_or(file_segment_ptr());
  dict_ = in.get_optional(&file_segment_ptr::from_xdr).value_or(file_segment_ptr());
  records_.decode(in);
  std::ignore = in.get_uint32(); // reserved_
}


} /* namespace monsoon::history::v2 */
