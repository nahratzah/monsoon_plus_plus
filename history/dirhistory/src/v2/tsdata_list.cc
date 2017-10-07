#include "tsdata_list.h"
#include <stack>
#include "../raw_file_segment_writer.h"

namespace monsoon {
namespace history {
namespace v2 {


tsdata_v2_list::tsdata_v2_list(file_segment<tsdata_list>&& data,
    const tsdata_v2::carg& constructor_arg)
: tsdata_v2(constructor_arg),
  data_(std::move(data))
{}

tsdata_v2_list::~tsdata_v2_list() noexcept {}

std::shared_ptr<io::fd> tsdata_v2_list::fd() const noexcept {
  return data_.ctx().fd();
}

bool tsdata_v2_list::is_writable() const noexcept {
  return fd()->can_write();
}

std::vector<time_series> tsdata_v2_list::read_all_raw_() const {
  std::vector<time_series> result;

  if (data_.file_ptr().offset() == 0u) // Empty file, bail out early
    return result;

  // Build stack of tsdata_list entries.
  std::stack<std::shared_ptr<const tsdata_list>> data_stack;
  data_stack.push(data_.get());
  const auto dict_ptr = data_stack.top()->dictionary();
  for (auto pred_ptr = data_stack.top()->pred();
      pred_ptr != nullptr;
      pred_ptr = data_stack.top()->pred()) {
    data_stack.push(pred_ptr);
  }

  // Process stack of tsdata_list entries.
  while (!data_stack.empty()) {
    const auto dptr = data_stack.top();
    data_stack.pop();

    time_series::tsv_set tsv_set;
    std::shared_ptr<const tsdata_list::record_array> records =
        dptr->records(*dict_ptr);
    std::transform(records->begin(), records->end(),
        std::inserter(tsv_set, tsv_set.end()),
        [](const auto& group_entry) {
          return time_series_value(
              std::get<0>(group_entry), // Group name
              *std::get<1>(group_entry).get()); // Metrics map
        });
    result.push_back(time_series(dptr->ts(), std::move(tsv_set)));
  }

  return result;
}

void tsdata_v2_list::push_back(const time_series& ts) {
  std::uint64_t write_start = hdr_file_size();
  const auto fd_ptr = fd();

  dictionary_delta dict = *data_.get()->dictionary();
  assert(!dict.update_pending());

  std::optional<file_segment_ptr> tsdata_pred;
  if (data_.file_ptr().offset() != 0u) tsdata_pred = data_.file_ptr();
  std::optional<file_segment_ptr> tsdata_dict; // XXX fill in
  file_segment_ptr tsdata_records; // XXX fill in

  io::fd::size_type dlen, slen;
  {
    auto tsfile_xdr = xdr::xdr_stream_writer<raw_file_segment_writer>(raw_file_segment_writer(*fd_ptr, write_start, &dlen, &slen));
    encode_timestamp(tsfile_xdr, ts.get_time());
    tsfile_xdr.put_optional(&encode_file_segment, tsdata_pred);
    tsfile_xdr.put_optional(&encode_file_segment, tsdata_dict);
    encode_file_segment(tsfile_xdr, tsdata_records);
    tsfile_xdr.put_uint32(0u); // reserved
    tsfile_xdr.close();
  }

  update_hdr(ts.get_time(), ts.get_time(), file_segment_ptr(write_start, dlen), write_start + slen);
  data_.update_addr(file_segment_ptr(write_start, dlen));
}


}}} /* namespace monsoon::history::v2 */
