#pragma once

#include "models.hpp"

#include <crow.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace library {

struct SessionData {
  std::optional<int> user_id;
  std::string role;
  std::vector<Flash> flashes;
};

class SessionStore {
 public:
  SessionData load(const crow::request& req, std::string& sid);
  void save(const std::string& sid, const SessionData& data);
  void clear(const std::string& sid);
  void flash(SessionData& data, const std::string& message, const std::string& category);
  std::vector<Flash> consume_flashes(SessionData& data);
  void attach_cookie(crow::response& res, const std::string& sid) const;

 private:
  std::string make_sid() const;
  std::optional<std::string> cookie_value(const crow::request& req, const std::string& name) const;

  std::unordered_map<std::string, SessionData> sessions_;
};

}  // namespace library
