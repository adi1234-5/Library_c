#pragma once

#include "database.hpp"
#include "session.hpp"

#include <crow.h>

namespace library {

void register_page_routes(crow::SimpleApp& app, Database& db, SessionStore& sessions);
void register_api_routes(crow::SimpleApp& app, Database& db, SessionStore& sessions);

IssueResult issue_book_for_user(Database& db, int user_id, const std::string& raw_book_id, int loan_days, bool enforce_visibility);
IssueResult return_issued_book(Database& db, int issue_id);

}  // namespace library
