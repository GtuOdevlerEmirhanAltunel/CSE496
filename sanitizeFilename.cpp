#include <algorithm>
#include <string>

std::string sanitizeFilename(const std::string& filename) {
  std::string sanitized = filename;
  // Remove path components
  size_t lastSlash = sanitized.find_last_of("/\\");
  if (lastSlash != std::string::npos) {
    sanitized = sanitized.substr(lastSlash + 1);
  }
  // Allow only alphanumeric, underscore, hyphen, dot
  sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
                                 [](char c) {
                                   return !std::isalnum(c) && c != '_' &&
                                          c != '-' && c != '.';
                                 }),
                  sanitized.end());
  return sanitized;
}
