#ifndef V1_TSDATA_H
#define V1_TSDATA_H

#include <monsoon/history/dir/tsdata.h>
#include <monsoon/simple_group.h>
#include <monsoon/metric_name.h>
#include <monsoon/tags.h>
#include <monsoon/group_name.h>
#include <monsoon/metric_value.h>
#include <monsoon/histogram.h>
#include <monsoon/time_series.h>
#include <monsoon/time_series_value.h>
#include <monsoon/time_point.h>
#include <monsoon/history/dir/dirhistory_export_.h>
#include <monsoon/xdr/xdr.h>

namespace monsoon {
namespace history {
namespace v1 {


template<typename T>
class monsoon_dirhistory_local_ dictionary {
 public:
  std::uint32_t encode(const T&);
  const T& decode(std::uint32_t) const;

  template<typename SerFn>
  void decode_update(xdr::xdr_istream&, SerFn);
  template<typename SerFn>
  void encode_update(xdr::xdr_ostream&, SerFn);

  bool update_pending() const noexcept;

 private:
  std::vector<T> decode_map_;
  std::unordered_map<T, std::uint32_t> encode_map_;
  std::uint32_t update_start_ = 0;
};

class monsoon_dirhistory_local_ dictionary_delta {
 public:
  void decode_update(xdr::xdr_istream&);
  void encode_update(xdr::xdr_ostream&);

  bool update_pending() const noexcept {
    return sdd.update_pending()
        || gdd.update_pending()
        || mdd.update_pending()
        || tdd.update_pending();
  }

  dictionary<std::string> sdd;
  dictionary<simple_group> gdd;
  dictionary<metric_name> mdd;
  dictionary<tags> tdd;
};

class monsoon_dirhistory_local_ tsdata_v1
: public tsdata,
  public std::enable_shared_from_this<tsdata_v1>
{
 public:
  static constexpr std::uint16_t MAJOR = 1u;
  static constexpr std::uint16_t MAX_MINOR = 0u;

  tsdata_v1(io::fd&& file);
  ~tsdata_v1() noexcept;

  auto read_all() const -> std::vector<time_series> override;
  static std::shared_ptr<tsdata_v1> write_all(const std::string&,
      std::vector<time_series>&&, bool);
  auto version() const noexcept -> std::tuple<std::uint16_t, std::uint16_t>
      override;
  bool is_writable() const noexcept override;
  void push_back(const time_series&) override;
  auto time() const -> std::tuple<time_point, time_point> override;

  static std::shared_ptr<tsdata_v1> new_file(io::fd&&, time_point tp);

 private:
  auto make_xdr_istream(bool) const -> std::unique_ptr<xdr::xdr_istream>;
  const dictionary_delta& get_dict_() const;
  dictionary_delta& get_dict_();
  void fault_dict_() const;

  io::fd file_;
  const bool gzipped_;
  time_point tp_begin_, tp_end_;
  std::uint16_t minor_version;
  mutable std::optional<dictionary_delta> dict_;
};


monsoon_dirhistory_local_
auto decode_tsfile_header(monsoon::xdr::xdr_istream&)
  -> std::tuple<time_point, time_point>;
monsoon_dirhistory_local_
void encode_tsfile_header(monsoon::xdr::xdr_ostream&,
    std::tuple<time_point, time_point>);

monsoon_dirhistory_local_
std::vector<std::string> decode_path(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_path(monsoon::xdr::xdr_ostream&, const std::vector<std::string>&);

monsoon_dirhistory_local_
metric_value decode_metric_value(monsoon::xdr::xdr_istream&,
    const dictionary<std::string>&);
monsoon_dirhistory_local_
void encode_metric_value(monsoon::xdr::xdr_ostream&,
    const metric_value& value, dictionary<std::string>&);

monsoon_dirhistory_local_
histogram decode_histogram(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_histogram(monsoon::xdr::xdr_ostream&, const histogram&);

monsoon_dirhistory_local_
tags decode_tags(monsoon::xdr::xdr_istream&, const dictionary<std::string>&);
monsoon_dirhistory_local_
void encode_tags(monsoon::xdr::xdr_ostream&, const tags&,
    dictionary<std::string>&);

monsoon_dirhistory_local_
time_series_value decode_time_series_value(xdr::xdr_istream&,
    const dictionary_delta&);
monsoon_dirhistory_local_
void encode_time_series_value(xdr::xdr_ostream&,
    const time_series_value&,
    dictionary_delta&);

monsoon_dirhistory_local_
time_point decode_timestamp(monsoon::xdr::xdr_istream&);
monsoon_dirhistory_local_
void encode_timestamp(monsoon::xdr::xdr_ostream&, const time_point&);

monsoon_dirhistory_local_
time_series decode_time_series(xdr::xdr_istream&,
    dictionary_delta&);
monsoon_dirhistory_local_
void encode_time_series(xdr::xdr_ostream&,
    const time_series&,
    dictionary_delta&);


}}} /* namespace monsoon::history::v1 */

#endif /* V1_TSDATA_H */
