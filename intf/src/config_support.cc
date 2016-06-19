#include <monsoon/config_support.h>
#include <array>
#include <codecvt>
#include <iomanip>
#include <locale>
#include <string>
#include <sstream>

namespace monsoon {
namespace {


constexpr std::array<const char*, 32> lo_escapes = {{
  "\\0"  , "\\001", "\\002", "\\003", "\\004", "\\005", "\\006", "\\a"  ,
  "\\b"  , "\\t"  , "\\n"  , "\\v"  , "\\f"  , "\\r"  , "\\016", "\\017",
  "\\020", "\\021", "\\022", "\\023", "\\024", "\\025", "\\026", "\\027",
  "\\030", "\\031", "\\032", "\\033", "\\034", "\\035", "\\036", "\\037",
}};


} /* namespace monsoon::<unnamed> */


auto quoted_string(const std::string& s) -> std::string {
  using converter =
      std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>;
  const std::u32string codepoints = converter().from_bytes(s);

  std::ostringstream result;
  result << std::right << std::setfill('0');
  for (char32_t c : codepoints) {
    if (c < lo_escapes.size()) {
      result << lo_escapes[c];
    } else if (c > 65535) {
      result << "\\U" << std::setw(8) << std::hex << (unsigned long)c;
    } else if (c > 127) {
      result << "\\u" << std::setw(4) << std::hex << (unsigned long)c;
    } else if (c == '"') {
      result << '\\' << (char)c;
    } else {
      result << (char)c;
    }
  }
  return result.str();
}


} /* namespace monsoon */
