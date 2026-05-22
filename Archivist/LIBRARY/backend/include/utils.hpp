#pragma once

#include "models.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace library {

std::string env_or(const char* name, const std::string& fallback);
int env_int_or(const char* name, int fallback);
double env_double_or(const char* name, double fallback);
std::string html_escape(const std::string& value);
std::string url_decode(const std::string& value);
std::unordered_map<std::string, std::string> parse_form_urlencoded(const std::string& body);
std::string get_form(const std::unordered_map<std::string, std::string>& form, const std::string& key, const std::string& fallback = "");
std::string query_param(const std::string& raw_url, const std::string& key);
std::string today_iso();
std::string add_days_iso(const std::string& iso_date, int days);
int days_between(const std::string& lhs_iso, const std::string& rhs_iso);
int parse_loan_days(const std::string& raw, int fallback);
std::string format_money(double value);
std::string title_case_role(const std::string& role);
std::string route_url(const std::string& endpoint);
std::string route_url(const std::string& endpoint, int id);
std::string read_text_file(const std::filesystem::path& path);

}  // namespace library
