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


constexpr std::array<const char*, 32> lo_escapes = {{
  "\\0"  , "\\001", "\\002", "\\003", "\\004", "\\005", "\\006", "\\a"  ,
  "\\b"  , "\\t"  , "\\n"  , "\\v"  , "\\f"  , "\\r"  , "\\016", "\\017",
  "\\020", "\\021", "\\022", "\\023", "\\024", "\\025", "\\026", "\\027",
  "\\030", "\\031", "\\032", "\\033", "\\034", "\\035", "\\036", "\\037",
}};

auto quote(const std::string& s, const char* special)
->  std::tuple<std::string, bool> {
  using converter =
      std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>;
  const std::u32string codepoints = converter().from_bytes(s);

  bool needs_quotes = false;
  std::ostringstream result;
  result << std::right << std::setfill('0');
  for (char32_t c : codepoints) {
    if (c < lo_escapes.size()) {
      result << lo_escapes[c];
      needs_quotes = true;
    } else if (c > 65535) {
      result << "\\U" << std::setw(8) << std::hex << (unsigned long)c;
      needs_quotes = true;
    } else if (c > 127) {
      result << "\\u" << std::setw(4) << std::hex << (unsigned long)c;
      needs_quotes = true;
    } else if (strchr(special, c) != nullptr) {
      result << '\\' << (char)c;
      needs_quotes = true;
    } else {
      result << (char)c;
    }
  }
  return std::make_tuple(result.str(), needs_quotes);
}


} /* namespace monsoon::<unnamed> */


auto quoted_string(const std::string& s) -> std::string {
  return "\"" + std::get<0>(quote(s, "\"")) + "\"";
}

auto maybe_quote_identifier(const std::string& s) -> std::string {
  auto identifier_re = std::regex("[_a-zA-Z][_a-zA-Z0-9]*");
  bool ident_quotes = !std::regex_match(s, identifier_re);

  std::string result;
  bool needs_quotes;
  std::tie(result, needs_quotes) = quote(s, "'");

  if (needs_quotes || ident_quotes) {
    result.insert(0, "'");
    result.append("'");
  }
  return result;
}


} /* namespace monsoon */
