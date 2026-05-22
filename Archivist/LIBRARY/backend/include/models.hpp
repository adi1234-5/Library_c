#pragma once

#include <map>
#include <string>
#include <vector>

namespace library {

using Row = std::map<std::string, std::string>;
using Rows = std::vector<Row>;

struct User {
  int id = 0;
  std::string username;
  std::string role;
};

struct Flash {
  std::string category;
  std::string message;
};

struct IssueResult {
  bool ok = false;
  std::string message;
  double fine = 0.0;
};

}  // namespace library
