#include <monsoon/metric_value.h>

namespace monsoon {


auto metric_value::operator==(const metric_value& other) const noexcept
->  bool {
  return value_ == other.value_;
}

auto metric_value::as_number() const noexcept
->  optional<any<long, unsigned long, double>> {
  return map(get(),
      [](const types& v) {
        return map_onto<optional<any<long, unsigned long, double>>>(v,
            [](bool b) {
              return any<long, unsigned long, double>::create<1>(b ? 1l : 0l);
            },
            [](long v) {
              return any<long, unsigned long, double>::create<0>(v);
            },
            [](unsigned long v) {
              return any<long, unsigned long, double>::create<1>(v);
            },
            [](double v) {
              return any<long, unsigned long, double>::create<2>(v);
            },
            [](const std::string& v) {
              return optional<any<long, unsigned long, double>>();
            });
      });
}


} /* namespace monsoon */


namespace std {


auto std::hash<monsoon::metric_value>::operator()(
    const monsoon::metric_value& v) const noexcept
->  size_t {
  if (!v.get().is_present()) return 0u;

  return monsoon::map_onto<size_t>(*v.get(),
                                   std::hash<bool>(),
                                   std::hash<long>(),
                                   std::hash<unsigned long>(),
                                   std::hash<double>(),
                                   std::hash<std::string>());
}


} /* namespace std */
