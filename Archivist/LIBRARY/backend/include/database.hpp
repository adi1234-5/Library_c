#pragma once

#include "app_config.hpp"
#include "models.hpp"

#include <sqlite3.h>

#include <optional>
#include <string>
#include <vector>

namespace library {

class Database {
 public:
  explicit Database(AppConfig config);
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  void ensure_initialized();
  Rows query(const std::string& sql, const std::vector<std::string>& params = {});
  int execute(const std::string& sql, const std::vector<std::string>& params = {});
  sqlite3_int64 last_insert_id() const;

  const AppConfig& config() const { return config_; }

 private:
  void open();
  void seed_user(const std::string& username, const std::string& password, const std::string& role);

  AppConfig config_;
  sqlite3* db_ = nullptr;
};

}  // namespace library
