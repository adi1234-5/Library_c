#pragma once

#include "app_config.hpp"
#include "models.hpp"

#include <optional>
#include <string>

namespace library {

std::string render_flash_messages(const std::vector<Flash>& flashes);
std::string render_home(const AppConfig& config, const std::optional<User>& user, const std::vector<Flash>& flashes);
std::string render_login(const AppConfig& config, const std::vector<Flash>& flashes);
std::string render_admin_inventory(const AppConfig& config, const User& user, const Rows& books, const Rows& issued_overview, const std::vector<Flash>& flashes);
std::string render_admin_circulation(const AppConfig& config, const User& user, const Rows& active_issues, const Rows& transactions, const Rows& available_books, const Rows& students, const std::vector<Flash>& flashes);
std::string render_search_books(const AppConfig& config, const User& user, const Rows& books, const std::string& q, const std::vector<Flash>& flashes);
std::string render_student_dashboard(const AppConfig& config, const User& user, const Rows& active_issues, const Rows& history, int total_borrowed, int due_soon, int overdue, double total_fines, const std::string& today, const std::vector<Flash>& flashes);

}  // namespace library
