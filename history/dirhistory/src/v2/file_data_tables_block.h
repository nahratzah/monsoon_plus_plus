#ifndef V2_FILE_DATA_TABLES_BLOCK_H
#define V2_FILE_DATA_TABLES_BLOCK_H

#include <monsoon/history/dir/dirhistory_export_.h>
#include "fwd.h"
#include "encdec_ctx.h"
#include "cache.h"
#include "dictionary.h"
#include "timestamp_delta.h"
#include "../dynamics.h"
#include <memory>
#include <monsoon/xdr/xdr.h>
#include <monsoon/time_point.h>

namespace monsoon::history::v2 {


monsoon_dirhistory_local_
auto decode(const cache_search_type<dictionary, file_data_tables_block>& cst, dictionary::allocator_type alloc) -> std::shared_ptr<dictionary>;


class monsoon_dirhistory_local_ file_data_tables_block
: public dynamics
{
 public:
  explicit file_data_tables_block(std::shared_ptr<file_data_tables> owner)
  : owner_(owner)
  {}

  ~file_data_tables_block() noexcept override;

  auto get_dictionary() -> std::shared_ptr<dictionary>;
  auto get_dictionary() const -> std::shared_ptr<const dictionary>;
  auto get_ctx() const -> encdec_ctx;

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

  auto decode(xdr::xdr_istream& in) -> void;
  auto encode(xdr::xdr_ostream& out) const -> void;

 protected:
  auto shared_from_this() -> std::shared_ptr<file_data_tables_block>;
  auto shared_from_this() const -> std::shared_ptr<const file_data_tables_block>;

 private:
  timestamp_delta timestamps_;
  file_segment_ptr dict_;
  file_segment_ptr tables_;
  std::weak_ptr<file_data_tables> owner_;
};


} /* namespace monsoon::history::v2 */

#endif /* V2_FILE_DATA_TABLES_BLOCK_H */
