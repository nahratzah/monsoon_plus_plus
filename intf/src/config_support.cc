#include <monsoon/config_support.h>
#include <array>
#include <codecvt>
#include <iomanip>
#include <locale>
#include <regex>
#include <string>
#include <sstream>
#include <tuple>

namespace monsoon {
namespace {


using namespace std::literals;

constexpr std::array<std::string_view, 32> lo_escapes{{
  "\\0"sv  , "\\001"sv, "\\002"sv, "\\003"sv, "\\004"sv, "\\005"sv, "\\006"sv, "\\a"sv  ,
  "\\b"sv  , "\\t"sv  , "\\n"sv  , "\\v"sv  , "\\f"sv  , "\\r"sv  , "\\016"sv, "\\017"sv,
  "\\020"sv, "\\021"sv, "\\022"sv, "\\023"sv, "\\024"sv, "\\025"sv, "\\026"sv, "\\027"sv,
  "\\030"sv, "\\031"sv, "\\032"sv, "\\033"sv, "\\034"sv, "\\035"sv, "\\036"sv, "\\037"sv,
}};

auto quote(std::string_view s, std::u32string_view special)
->  std::tuple<std::string, bool> {
  using converter =
      std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>;
  const std::u32string codepoints = converter().from_bytes(std::string(s));

  bool needs_quotes = false;
  std::ostringstream result;
  result << std::right << std::setfill('0');
  for (char32_t c : codepoints) {
    if (c < lo_escapes.size()) {
      result << lo_escapes[c];
      needs_quotes = true;
    } else if (c == '\\') {
      result << R"(\\)"sv;
      needs_quotes = true;
    } else if (c > 65535) {
      result << "\\U" << std::setw(8) << std::hex << (unsigned long)c;
      needs_quotes = true;
    } else if (c > 127) {
      result << "\\u" << std::setw(4) << std::hex << (unsigned long)c;
      needs_quotes = true;
    } else if (special.find(c) != std::string_view::npos) {
      result << '\\' << (char)c;
      needs_quotes = true;
    } else {
      result << (char)c;
    }
  }
  return std::make_tuple(result.str(), needs_quotes);
}


} /* namespace monsoon::<unnamed> */


auto quoted_string(std::string_view s) -> std::string {
  return "\"" + std::get<0>(quote(s, U"\"")) + "\"";
}

auto maybe_quote_identifier(std::string_view s) -> std::string {
  auto identifier_re = std::regex("[_a-zA-Z][_a-zA-Z0-9]*");
  bool ident_quotes = !std::regex_match(std::string(s), identifier_re);

  std::string result;
  bool needs_quotes;
  std::tie(result, needs_quotes) = quote(s, U"'");

  if (needs_quotes || ident_quotes) {
    result.insert(0, "'");
    result.append("'");
  }
  return result;
}


} /* namespace monsoon */
