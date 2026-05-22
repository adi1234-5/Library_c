#include "database.hpp"

#include "auth.hpp"
#include "utils.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace library {

AppConfig AppConfig::from_environment() {
  AppConfig config;
  config.root_path = std::filesystem::current_path();
  config.database_path = env_or("LIBRARY_DB_PATH", (config.root_path / "database" / "library.db").string());
  config.schema_path = config.root_path / "database" / "schema.sql";
  config.templates_path = config.root_path / "frontend" / "templates";
  config.secret_key = env_or("SECRET_KEY", "change-me-in-production");
  config.port = env_int_or("PORT", 5000);
  config.default_loan_days = env_int_or("DEFAULT_LOAN_DAYS", 14);
  config.fine_per_day = env_double_or("FINE_PER_DAY", 1.0);
  return config;
}

Database::Database(AppConfig config) : config_(std::move(config)) { open(); }

Database::~Database() {
  if (db_) sqlite3_close(db_);
}

void Database::open() {
  if (sqlite3_open(config_.database_path.string().c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("Could not open SQLite database");
  }
  execute("PRAGMA foreign_keys = ON");
}

void Database::ensure_initialized() {
  bool needs_seed = !std::filesystem::exists(config_.database_path) || query("SELECT name FROM sqlite_master WHERE type='table' AND name='users'").empty();
  std::string schema = read_text_file(config_.schema_path);
  char* error = nullptr;
  if (sqlite3_exec(db_, schema.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    std::string message = error ? error : "schema initialization failed";
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
  if (needs_seed) {
    seed_user("admin", "admin123", "admin");
    seed_user("student", "student123", "student");
  }
}

void Database::seed_user(const std::string& username, const std::string& password, const std::string& role) {
  if (!query("SELECT id FROM users WHERE username = ?", {username}).empty()) return;
  execute("INSERT INTO users (username, password, role) VALUES (?, ?, ?)", {username, generate_password_hash(password), role});
}

Rows Database::query(const std::string& sql, const std::vector<std::string>& params) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }
  for (int i = 0; i < static_cast<int>(params.size()); ++i) {
    sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
  }
  Rows rows;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    Row row;
    int columns = sqlite3_column_count(stmt);
    for (int i = 0; i < columns; ++i) {
      const char* name = sqlite3_column_name(stmt, i);
      const unsigned char* text = sqlite3_column_text(stmt, i);
      row[name ? name : ""] = text ? reinterpret_cast<const char*>(text) : "";
    }
    rows.push_back(std::move(row));
  }
  sqlite3_finalize(stmt);
  return rows;
}

int Database::execute(const std::string& sql, const std::vector<std::string>& params) {
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite3_errmsg(db_));
  }
  for (int i = 0; i < static_cast<int>(params.size()); ++i) {
    sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
  }
  int rc = sqlite3_step(stmt);
  if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
    std::string message = sqlite3_errmsg(db_);
    sqlite3_finalize(stmt);
    throw std::runtime_error(message);
  }
  int changes = sqlite3_changes(db_);
  sqlite3_finalize(stmt);
  return changes;
}

sqlite3_int64 Database::last_insert_id() const {
  return sqlite3_last_insert_rowid(db_);
}

}  // namespace library
