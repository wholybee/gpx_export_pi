/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   XML utility functions - implementation                                *
 **************************************************************************/

#include "xml_util.h"

#include <cstring>
#include <iomanip>
#include <sstream>

namespace xml_util {

std::string EscapeXml(const std::string& input) {
  std::string result;
  result.reserve(input.size() + 16);
  for (char c : input) {
    switch (c) {
      case '&':
        result += "&amp;";
        break;
      case '<':
        result += "&lt;";
        break;
      case '>':
        result += "&gt;";
        break;
      case '"':
        result += "&quot;";
        break;
      case '\'':
        result += "&apos;";
        break;
      default:
        // Drop control characters except tab, newline, carriage return
        if (c >= 0x20 || c == '\t' || c == '\n' || c == '\r') {
          result += c;
        }
        break;
    }
  }
  return result;
}

std::string FormatLat(double lat) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(9) << lat;
  return ss.str();
}

std::string FormatLon(double lon) {
  std::ostringstream ss;
  ss << std::fixed << std::setprecision(9) << lon;
  return ss.str();
}

std::string FormatTimeISO8601(std::time_t t) {
  struct tm utc;
#if defined(_WIN32) || defined(_WIN64)
  gmtime_s(&utc, &t);
#else
  gmtime_r(&t, &utc);
#endif
  char buf[32];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return std::string(buf);
}

}  // namespace xml_util
