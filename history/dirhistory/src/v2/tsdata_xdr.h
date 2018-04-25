#ifndef V2_TSDATA_XDR_H
#define V2_TSDATA_XDR_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "../dynamics.h"
#include "dictionary.h"
#include "encdec_ctx.h"
#include "file_segment_ptr.h"
#include "fwd.h"
#include "cache.h"
#include <cstdint>
#include <optional>
#include <memory>
#include <monsoon/time_point.h>
#include <monsoon/xdr/xdr.h>

namespace monsoon::history::v2 {


monsoon_dirhistory_local_
auto decode(const cache_search_type<dictionary, tsdata_xdr>& cst, dictionary::allocator_type alloc) -> std::shared_ptr<dictionary>;


class monsoon_dirhistory_local_ tsdata_xdr
: public std::enable_shared_from_this<tsdata_xdr>,
  public typed_dynamics<dynamics>
{
  friend auto decode(const cache_search_type<dictionary, tsdata_xdr>& cst, dictionary::allocator_type alloc) -> std::shared_ptr<dictionary>;

 public:
  using allocator_type = cache_allocator<int>; // Unused.

  static constexpr bool is_compressed = false;

  explicit tsdata_xdr(std::shared_ptr<tsdata_v2> parent, allocator_type alloc = allocator_type());
  explicit tsdata_xdr(std::shared_ptr<tsdata_xdr> parent, allocator_type alloc = allocator_type());
  ~tsdata_xdr() noexcept override;

  auto get_dictionary() const -> std::shared_ptr<const dictionary>;
  auto get_dictionary() -> std::shared_ptr<dictionary>;

  auto get_ctx() const
  -> encdec_ctx {
    return ctx_;
  }

  auto get_predecessor() const -> std::shared_ptr<const tsdata_xdr>;
  auto get_predecessor() -> std::shared_ptr<tsdata_xdr>;
  auto get() const -> std::shared_ptr<const record_array>;
  auto ts() const noexcept -> time_point { return ts_; }

  auto decode(xdr::xdr_istream& in) -> void;

 private:
  time_point ts_;
  file_segment_ptr pred_; // May be nil.
  file_segment_ptr dict_; // May be nil.
  file_segment_ptr records_;
  encdec_ctx ctx_;
};


} /* namespace monsoon::history::v2 */

#endif /* V2_TSDATA_XDR_H */
