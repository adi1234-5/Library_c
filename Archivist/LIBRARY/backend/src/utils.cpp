#include "utils.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace library {

std::string env_or(const char* name, const std::string& fallback) {
  const char* value = std::getenv(name);
  return value && *value ? std::string(value) : fallback;
}

int env_int_or(const char* name, int fallback) {
  try { return std::stoi(env_or(name, std::to_string(fallback))); } catch (...) { return fallback; }
}

double env_double_or(const char* name, double fallback) {
  try { return std::stod(env_or(name, std::to_string(fallback))); } catch (...) { return fallback; }
}

std::string html_escape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }
  return out;
}

std::string url_decode(const std::string& value) {
  std::string out;
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '+') {
      out += ' ';
    } else if (value[i] == '%' && i + 2 < value.size()) {
      int byte = 0;
      std::istringstream hex(value.substr(i + 1, 2));
      if (hex >> std::hex >> byte) {
        out += static_cast<char>(byte);
        i += 2;
      } else {
        out += value[i];
      }
    } else {
      out += value[i];
    }
  }
  return out;
}

std::unordered_map<std::string, std::string> parse_form_urlencoded(const std::string& body) {
  std::unordered_map<std::string, std::string> form;
  size_t start = 0;
  while (start <= body.size()) {
    size_t amp = body.find('&', start);
    std::string part = body.substr(start, amp == std::string::npos ? std::string::npos : amp - start);
    size_t eq = part.find('=');
    if (eq != std::string::npos) {
      form[url_decode(part.substr(0, eq))] = url_decode(part.substr(eq + 1));
    }
    if (amp == std::string::npos) break;
    start = amp + 1;
  }
  return form;
}

std::string get_form(const std::unordered_map<std::string, std::string>& form, const std::string& key, const std::string& fallback) {
  auto it = form.find(key);
  return it == form.end() ? fallback : it->second;
}

std::string query_param(const std::string& raw_url, const std::string& key) {
  size_t q = raw_url.find('?');
  if (q == std::string::npos) return "";
  auto params = parse_form_urlencoded(raw_url.substr(q + 1));
  return get_form(params, key);
}

static std::chrono::sys_days parse_iso_day(const std::string& iso) {
  int y = 1970, m = 1, d = 1;
  if (iso.size() >= 10) {
    y = std::stoi(iso.substr(0, 4));
    m = std::stoi(iso.substr(5, 2));
    d = std::stoi(iso.substr(8, 2));
  }
  return std::chrono::year{y} / std::chrono::month{static_cast<unsigned>(m)} / std::chrono::day{static_cast<unsigned>(d)};
}

static std::string format_day(std::chrono::sys_days day) {
  auto ymd = std::chrono::year_month_day{day};
  std::ostringstream out;
  out << static_cast<int>(ymd.year()) << '-'
      << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ymd.month()) << '-'
      << std::setw(2) << std::setfill('0') << static_cast<unsigned>(ymd.day());
  return out.str();
}

std::string today_iso() {
  auto now = std::chrono::system_clock::now();
  auto day = std::chrono::floor<std::chrono::days>(now);
  return format_day(day);
}

std::string add_days_iso(const std::string& iso_date, int days) {
  return format_day(parse_iso_day(iso_date) + std::chrono::days(days));
}

int days_between(const std::string& lhs_iso, const std::string& rhs_iso) {
  return static_cast<int>((parse_iso_day(lhs_iso) - parse_iso_day(rhs_iso)).count());
}

int parse_loan_days(const std::string& raw, int fallback) {
  try {
    int days = std::stoi(raw);
    return (days == 7 || days == 14) ? days : fallback;
  } catch (...) {
    return fallback;
  }
}

std::string format_money(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << value;
  return out.str();
}

std::string title_case_role(const std::string& role) {
  if (role == "admin") return "Admin";
  if (role == "student") return "Student";
  return role;
}

std::string route_url(const std::string& endpoint) {
  static const std::unordered_map<std::string, std::string> routes = {
      {"home", "/"}, {"dashboard", "/dashboard"}, {"login", "/login"}, {"signup", "/signup"},
      {"logout", "/logout"}, {"admin_inventory", "/admin/inventory"}, {"add_book", "/admin/books/add"},
      {"admin_circulation", "/admin/circulation"}, {"admin_issue_book", "/admin/issue"},
      {"search_books", "/books/search"}, {"student_dashboard", "/student/dashboard"}};
  auto it = routes.find(endpoint);
  return it == routes.end() ? "/" : it->second;
}

std::string route_url(const std::string& endpoint, int id) {
  if (endpoint == "edit_book") return "/admin/books/" + std::to_string(id) + "/edit";
  if (endpoint == "delete_book") return "/admin/books/" + std::to_string(id) + "/delete";
  if (endpoint == "toggle_book_visibility") return "/admin/books/" + std::to_string(id) + "/visibility";
  if (endpoint == "admin_return_book") return "/admin/return/" + std::to_string(id);
  if (endpoint == "student_issue_book") return "/student/issue/" + std::to_string(id);
  if (endpoint == "student_return_book") return "/student/return/" + std::to_string(id);
  return route_url(endpoint);
}

std::string read_text_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace library
