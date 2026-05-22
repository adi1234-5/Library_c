#include "auth.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <iomanip>
#include <sstream>
#include <vector>

namespace library {

static std::string to_hex(const unsigned char* data, size_t len) {
  std::ostringstream out;
  for (size_t i = 0; i < len; ++i) {
    out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
  }
  return out.str();
}

static std::vector<unsigned char> from_hex(const std::string& hex) {
  std::vector<unsigned char> bytes;
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    bytes.push_back(static_cast<unsigned char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
  }
  return bytes;
}

static std::vector<std::string> split(const std::string& value, char delimiter) {
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= value.size()) {
    size_t pos = value.find(delimiter, start);
    parts.push_back(value.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
    if (pos == std::string::npos) break;
    start = pos + 1;
  }
  return parts;
}

std::string generate_password_hash(const std::string& password) {
  constexpr int n = 32768;
  constexpr int r = 8;
  constexpr int p = 1;
  unsigned char salt_bytes[12];
  RAND_bytes(salt_bytes, sizeof(salt_bytes));
  std::string salt = to_hex(salt_bytes, sizeof(salt_bytes)).substr(0, 16);
  unsigned char out[64];
  EVP_PBE_scrypt(password.c_str(), password.size(), reinterpret_cast<const unsigned char*>(salt.data()), salt.size(), n, r, p, 0, out, sizeof(out));
  return "scrypt:32768:8:1$" + salt + "$" + to_hex(out, sizeof(out));
}

bool check_password_hash(const std::string& stored_hash, const std::string& password) {
  auto parts = split(stored_hash, '$');
  if (parts.size() != 3) return false;
  auto method = split(parts[0], ':');
  if (method.size() != 4 || method[0] != "scrypt") return false;
  int n = std::stoi(method[1]);
  int r = std::stoi(method[2]);
  int p = std::stoi(method[3]);
  auto expected = from_hex(parts[2]);
  if (expected.empty()) return false;
  std::vector<unsigned char> actual(expected.size());
  if (EVP_PBE_scrypt(password.c_str(), password.size(), reinterpret_cast<const unsigned char*>(parts[1].data()), parts[1].size(), n, r, p, 0, actual.data(), actual.size()) != 1) {
    return false;
  }
  return CRYPTO_memcmp(expected.data(), actual.data(), expected.size()) == 0;
}

std::optional<User> current_user(Database& db, const SessionData& session) {
  if (!session.user_id) return std::nullopt;
  auto rows = db.query("SELECT id, username, role FROM users WHERE id = ?", {std::to_string(*session.user_id)});
  if (rows.empty()) return std::nullopt;
  return User{std::stoi(rows[0].at("id")), rows[0].at("username"), rows[0].at("role")};
}

}  // namespace library
