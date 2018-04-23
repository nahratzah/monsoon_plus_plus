#ifndef V2_FILE_DATA_TABLES_BLOCK_H
#define V2_FILE_DATA_TABLES_BLOCK_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "fwd.h"
#include "encdec_ctx.h"
#include "timestamp_delta.h"
#include "../dynamics.h"
#include <memory>
#include <monsoon/xdr/xdr.h>
#include <monsoon/time_point.h>

namespace monsoon::history::v2 {


class monsoon_dirhistory_local_ file_data_tables_block
: public typed_dynamics<void>,
  public std::enable_shared_from_this<file_data_tables_block>
{
 public:
  [[deprecated]]
  explicit file_data_tables_block(encdec_ctx ctx)
  : file_data_tables_block(nullptr, std::move(ctx))
  {}

  file_data_tables_block(std::shared_ptr<void> parent, encdec_ctx ctx)
  : typed_dynamics<void>(std::move(parent)),
    ctx_(std::move(ctx))
  {}

  ~file_data_tables_block() noexcept override;

  auto get_dictionary() -> std::shared_ptr<dictionary>;
  auto get_dictionary() const -> std::shared_ptr<const dictionary>;

  auto get_ctx() const
  -> const encdec_ctx& {
    return ctx_;
  }

  auto timestamps() const
  -> const timestamp_delta& {
    return timestamps_;
  }

  auto get() const -> std::shared_ptr<const tables>;

  auto time() const
  noexcept
  -> std::optional<std::tuple<time_point, time_point>> {
    if (timestamps_.empty()) return {};
    return std::make_tuple(timestamps_.front(), timestamps_.back());
  }

  static auto from_xdr(
      std::shared_ptr<void> parent,
      xdr::xdr_istream& in,
      encdec_ctx ctx)
      -> std::shared_ptr<file_data_tables_block>;
  auto decode(xdr::xdr_istream& in) -> void;
  auto encode(xdr::xdr_ostream& out) const -> void;

 private:
  timestamp_delta timestamps_;
  file_segment_ptr dict_;
  file_segment_ptr tables_;
  encdec_ctx ctx_;
};


} /* namespace monsoon::history::v2 */

#endif /* V2_FILE_DATA_TABLES_BLOCK_H */
