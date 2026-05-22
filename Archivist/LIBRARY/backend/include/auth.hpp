#pragma once

#include "database.hpp"
#include "models.hpp"
#include "session.hpp"

#include <crow.h>

#include <optional>
#include <string>

namespace library {

std::string generate_password_hash(const std::string& password);
bool check_password_hash(const std::string& stored_hash, const std::string& password);
std::optional<User> current_user(Database& db, const SessionData& session);

}  // namespace library
