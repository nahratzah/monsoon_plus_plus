#include <monsoon/history/dir/dirhistory.h>
#include <monsoon/history/collect_history.h>
#include <monsoon/time_point.h>
#include <monsoon/time_range.h>
#include <monsoon/expression.h>
#include <monsoon/tags.h>
#include <monsoon/metric_value.h>
#include <monsoon/overload.h>
#include <iostream>
#include <memory>
#include <algorithm>
#include <iostream>
#include <utility>
#include <sstream>
#include <string>

auto open_dir(std::string dir)
-> std::unique_ptr<monsoon::collect_history> {
  return std::make_unique<monsoon::history::dirhistory>(dir, false);
}

std::string concat_args(char** b, char** e) {
  std::ostringstream oss;
  if (b != e) oss << *b++;
  std::for_each(b, e, [&oss](std::string_view s) { oss << " " << s; });
  return std::move(oss).str();
}

auto print_scalar(monsoon::expression::scalar_objpipe&& pipe) -> monsoon::objpipe::reader<std::string>;
auto print_vector(monsoon::expression::vector_objpipe&& pipe) -> monsoon::objpipe::reader<std::string>;

int main(int argc, char** argv) {
  if (argc <= 2) {
    std::cerr << (argc >= 1 ? argv[0] : "eval")
        << " /path/to/history/dir expression...\n";
    return 1;
  }

  monsoon::expression_ptr expr_ptr =
      monsoon::expression::parse(concat_args(argv + 2, argv + argc));
  std::cout << "Evaluated expression: " << *expr_ptr << "\n";

  auto eval_stream_variant = (*expr_ptr) (
      *open_dir(argv[1]),
      monsoon::time_range(),
      monsoon::time_point::duration(5 * 60 * 1000));
  std::visit(
      monsoon::overload(&print_scalar, &print_vector),
      std::move(eval_stream_variant))
      .for_each([](const auto& v) { std::cout << v << "\n"; });
}

auto print_scalar(monsoon::expression::scalar_objpipe&& pipe)
-> monsoon::objpipe::reader<std::string> {
  return std::move(pipe)
      .filter([](const auto& v) { return v.data.index() == 1u; })
      .transform(
          [](const auto& v) {
            return (std::ostringstream() << v.tp << ": " << std::get<1>(v.data))
                .str();
          });
}

auto print_vector(monsoon::expression::vector_objpipe&& pipe)
-> monsoon::objpipe::reader<std::string> {
  return std::move(pipe)
      .filter([](const auto& v) { return v.data.index() == 1u; })
      .transform(
          [](const auto& v) {
            std::ostringstream out;
            out << v.tp << ":";
            for (const auto& entry : std::get<1>(v.data)) {
              const monsoon::tags tags = entry.first;
              const monsoon::metric_value value = entry.second;
              out << "\n" // newline separator
                  << "  " // indent
                  << tags
                  << "="
                  << value;
            }
            return out.str();
          });
}
