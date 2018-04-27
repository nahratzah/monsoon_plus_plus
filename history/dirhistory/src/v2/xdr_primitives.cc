#include "xdr_primitives.h"

namespace monsoon::history::v2 {


enum class metrickind : std::uint32_t {
  BOOL = 0,
  INT = 1,
  FLOAT = 2,
  STRING = 3,
  HISTOGRAM = 4,
  EMPTY = 0x7fffffff
};

metric_value decode_metric_value(monsoon::xdr::xdr_istream& in,
    const strval_dictionary& dict) {
  switch (in.get_uint32()) {
    default:
      throw xdr::xdr_exception();
    case static_cast<std::uint32_t>(metrickind::BOOL):
      return metric_value(in.get_bool());
      break;
    case static_cast<std::uint32_t>(metrickind::INT):
      return metric_value(in.get_int64());
      break;
    case static_cast<std::uint32_t>(metrickind::FLOAT):
      return metric_value(in.get_flt64());
      break;
    case static_cast<std::uint32_t>(metrickind::STRING):
      return metric_value(dict[in.get_uint32()]);
      break;
    case static_cast<std::uint32_t>(metrickind::HISTOGRAM):
      return metric_value(decode_histogram(in));
      break;
    case static_cast<std::uint32_t>(metrickind::EMPTY):
      return metric_value();
      break;
  }
}

void encode_metric_value(monsoon::xdr::xdr_ostream& out,
    const metric_value& value, strval_dictionary& dict) {
  visit(
      overload(
          [&out](const metric_value::empty&) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::EMPTY));
          },
          [&out](bool b) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::BOOL));
            out.put_bool(b);
          },
          [&out](const metric_value::signed_type& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::INT));
            out.put_int64(v);
          },
          [&out](const metric_value::unsigned_type& v) {
            if (v < static_cast<metric_value::unsigned_type>(std::numeric_limits<metric_value::signed_type>::max())) {
              out.put_uint32(static_cast<std::uint32_t>(metrickind::INT));
              out.put_int64(static_cast<std::int64_t>(v));
            } else { // Can't be represented in signed type.
              out.put_uint32(static_cast<std::uint32_t>(metrickind::FLOAT));
              out.put_flt64(v);
            }
          },
          [&out](const metric_value::fp_type& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::FLOAT));
            out.put_flt64(v);
          },
          [&out, &dict](const std::string_view& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::STRING));
            out.put_uint32(dict[v]);
          },
          [&out](const histogram& v) {
            out.put_uint32(static_cast<std::uint32_t>(metrickind::HISTOGRAM));
            encode_histogram(out, v);
          }),
      value.get());
}

histogram decode_histogram(monsoon::xdr::xdr_istream& in) {
  histogram result;
  in.accept_collection(
      [](monsoon::xdr::xdr_istream& in) {
        const double lo = in.get_flt64();
        const double hi = in.get_flt64();
        const double count = in.get_flt64();
        return std::make_tuple(histogram::range(lo, hi), count);
      },
      [&result](const std::tuple<histogram::range, double>& v) {
        result.add(std::get<0>(v), std::get<1>(v));
      });
  return result;
}

void encode_histogram(monsoon::xdr::xdr_ostream& out, const histogram& hist) {
  const auto& data = hist.data();
  out.put_collection(
      [](monsoon::xdr::xdr_ostream& out, const auto& hist_elem) {
        out.put_flt64(std::get<0>(hist_elem).low());
        out.put_flt64(std::get<0>(hist_elem).high());
        out.put_flt64(std::get<1>(hist_elem));
      },
      data.begin(), data.end());
}


} /* namespace monsoon::history::v2 */
