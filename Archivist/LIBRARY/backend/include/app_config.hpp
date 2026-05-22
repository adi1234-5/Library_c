#pragma once

#include <filesystem>
#include <string>

namespace library {

struct AppConfig {
  std::filesystem::path root_path;
  std::filesystem::path database_path;
  std::filesystem::path schema_path;
  std::filesystem::path templates_path;
  std::string secret_key = "change-me-in-production";
  int port = 5000;
  int default_loan_days = 14;
  double fine_per_day = 1.0;

  static AppConfig from_environment();
};

}  // namespace library
