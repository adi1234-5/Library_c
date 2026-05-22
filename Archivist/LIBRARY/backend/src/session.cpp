#include "session.hpp"

#include <openssl/rand.h>

#include <iomanip>
#include <sstream>

namespace library {

static std::string hex_token(size_t bytes_count) {
  std::vector<unsigned char> bytes(bytes_count);
  RAND_bytes(bytes.data(), static_cast<int>(bytes.size()));
  std::ostringstream out;
  for (unsigned char byte : bytes) {
    out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
  }
  return out.str();
}

std::string SessionStore::make_sid() const {
  return hex_token(32);
}

std::optional<std::string> SessionStore::cookie_value(const crow::request& req, const std::string& name) const {
  auto header = req.get_header_value("Cookie");
  size_t start = 0;
  while (start < header.size()) {
    while (start < header.size() && (header[start] == ' ' || header[start] == ';')) ++start;
    size_t eq = header.find('=', start);
    if (eq == std::string::npos) break;
    size_t end = header.find(';', eq + 1);
    std::string key = header.substr(start, eq - start);
    if (key == name) return header.substr(eq + 1, end == std::string::npos ? std::string::npos : end - eq - 1);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return std::nullopt;
}

SessionData SessionStore::load(const crow::request& req, std::string& sid) {
  auto existing = cookie_value(req, "library_session");
  sid = existing.value_or(make_sid());
  auto found = sessions_.find(sid);
  return found == sessions_.end() ? SessionData{} : found->second;
}

void SessionStore::save(const std::string& sid, const SessionData& data) {
  sessions_[sid] = data;
}

void SessionStore::clear(const std::string& sid) {
  sessions_[sid] = SessionData{};
}

void SessionStore::flash(SessionData& data, const std::string& message, const std::string& category) {
  data.flashes.push_back({category, message});
}

std::vector<Flash> SessionStore::consume_flashes(SessionData& data) {
  auto flashes = data.flashes;
  data.flashes.clear();
  return flashes;
}

void SessionStore::attach_cookie(crow::response& res, const std::string& sid) const {
  res.add_header("Set-Cookie", "library_session=" + sid + "; HttpOnly; Path=/; SameSite=Lax");
}

}  // namespace library
