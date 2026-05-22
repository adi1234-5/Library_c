#include "handlers.hpp"

#include "auth.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace library {
namespace {

using json = nlohmann::json;

crow::response json_response(const json& body, int code, SessionStore& sessions, const std::string& sid, const SessionData& session) {
  crow::response res(code, body.dump());
  res.set_header("Content-Type", "application/json");
  sessions.save(sid, session);
  sessions.attach_cookie(res, sid);
  return res;
}

std::pair<std::optional<User>, std::optional<crow::response>> api_auth(Database& db, SessionStore& sessions, SessionData& session, const std::string& sid, const std::optional<std::string>& role = std::nullopt) {
  auto user = current_user(db, session);
  if (!user) return {std::nullopt, json_response({{"error", "Authentication required"}}, 401, sessions, sid, session)};
  if (role && user->role != *role) return {std::nullopt, json_response({{"error", "Forbidden"}}, 403, sessions, sid, session)};
  return {user, std::nullopt};
}

json rows_json(const Rows& rows) {
  json out = json::array();
  for (const auto& row : rows) {
    json item = json::object();
    for (const auto& [key, value] : row) {
      if (key == "id" || key == "user_id" || key == "book_id" || key == "visibility") item[key] = value.empty() ? 0 : std::stoi(value);
      else if (key == "fine") item[key] = value.empty() ? 0.0 : std::stod(value);
      else item[key] = value;
    }
    out.push_back(item);
  }
  return out;
}

bool json_bool(const json& payload, const std::string& key, bool fallback) {
  if (!payload.contains(key)) return fallback;
  if (payload[key].is_boolean()) return payload[key].get<bool>();
  if (payload[key].is_number_integer()) return payload[key].get<int>() != 0;
  if (payload[key].is_string()) return payload[key].get<std::string>() == "1" || payload[key].get<std::string>() == "true";
  return fallback;
}

std::string json_string(const json& payload, const std::string& key, const std::string& fallback = "") {
  if (!payload.contains(key) || payload[key].is_null()) return fallback;
  if (payload[key].is_string()) return payload[key].get<std::string>();
  return payload[key].dump();
}

}  // namespace

void register_api_routes(crow::SimpleApp& app, Database& db, SessionStore& sessions) {
  CROW_ROUTE(app, "/api/signup").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    json payload = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
    if (payload.is_discarded()) payload = json::object();
    std::string username = json_string(payload, "username");
    std::string password = json_string(payload, "password");
    std::string role = json_string(payload, "role", "student");
    if (role != "admin" && role != "student") role = "student";
    if (username.empty() || password.empty()) return json_response({{"error", "username and password are required"}}, 400, sessions, sid, session);
    if (!db.query("SELECT id FROM users WHERE username = ?", {username}).empty()) return json_response({{"error", "username already exists"}}, 409, sessions, sid, session);
    db.execute("INSERT INTO users (username, password, role) VALUES (?, ?, ?)", {username, generate_password_hash(password), role});
    return json_response({{"message", "signup successful"}, {"username", username}, {"role", role}}, 201, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/login").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    json payload = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
    if (payload.is_discarded()) payload = json::object();
    std::string username = json_string(payload, "username");
    std::string password = json_string(payload, "password");
    if (username.empty() || password.empty()) return json_response({{"error", "username and password are required"}}, 400, sessions, sid, session);
    auto rows = db.query("SELECT id, username, password, role FROM users WHERE username = ?", {username});
    if (rows.empty() || !check_password_hash(rows[0]["password"], password)) return json_response({{"error", "invalid username or password"}}, 401, sessions, sid, session);
    session = SessionData{};
    session.user_id = std::stoi(rows[0]["id"]);
    session.role = rows[0]["role"];
    return json_response({{"message", "login successful"}, {"user", {{"id", std::stoi(rows[0]["id"])}, {"username", rows[0]["username"]}, {"role", rows[0]["role"]}}}}, 200, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/logout").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    session = SessionData{};
    return json_response({{"message", "logout successful"}}, 200, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/books").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto [user, err] = api_auth(db, sessions, session, sid); if (err) return *err;
    std::string sql = "SELECT id, title, author, status, visibility FROM books";
    std::vector<std::string> params;
    std::string q = query_param(req.raw_url, "q");
    std::transform(q.begin(), q.end(), q.begin(), ::tolower);
    if (user->role == "student") {
      sql += " WHERE visibility = 1";
      if (!q.empty()) { sql += " AND (LOWER(title) LIKE ? OR LOWER(author) LIKE ?)"; params = {"%" + q + "%", "%" + q + "%"}; }
    } else if (!q.empty()) {
      sql += " WHERE (LOWER(title) LIKE ? OR LOWER(author) LIKE ?)";
      params = {"%" + q + "%", "%" + q + "%"};
    }
    sql += " ORDER BY title";
    return json_response({{"books", rows_json(db.query(sql, params))}}, 200, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/books").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto [user, err] = api_auth(db, sessions, session, sid, "admin"); if (err) return *err;
    json payload = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
    if (payload.is_discarded()) payload = json::object();
    std::string title = json_string(payload, "title");
    std::string author = json_string(payload, "author");
    std::string visibility = json_bool(payload, "visibility", true) ? "1" : "0";
    if (title.empty() || author.empty()) return json_response({{"error", "title and author are required"}}, 400, sessions, sid, session);
    db.execute("INSERT INTO books (title, author, status, visibility) VALUES (?, ?, 'Available', ?)", {title, author, visibility});
    return json_response({{"message", "book created"}, {"book_id", db.last_insert_id()}}, 201, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/books/<int>").methods(crow::HTTPMethod::DELETE)([&](const crow::request& req, int book_id) {
    std::string sid; auto session = sessions.load(req, sid);
    auto [user, err] = api_auth(db, sessions, session, sid, "admin"); if (err) return *err;
    if (!db.query("SELECT id FROM issued_books WHERE book_id = ? AND return_date IS NULL", {std::to_string(book_id)}).empty()) return json_response({{"error", "cannot delete currently issued book"}}, 400, sessions, sid, session);
    db.execute("DELETE FROM books WHERE id = ?", {std::to_string(book_id)});
    return json_response({{"message", "book deleted"}}, 200, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/books/<int>").methods(crow::HTTPMethod::PUT)([&](const crow::request& req, int book_id) {
    std::string sid; auto session = sessions.load(req, sid);
    auto [user, err] = api_auth(db, sessions, session, sid, "admin"); if (err) return *err;
    json payload = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
    if (payload.is_discarded()) payload = json::object();
    std::string title = json_string(payload, "title");
    std::string author = json_string(payload, "author");
    std::string status = json_string(payload, "status", "Available");
    if (status != "Available" && status != "Issued") return json_response({{"error", "invalid status"}}, 400, sessions, sid, session);
    if (title.empty() || author.empty()) return json_response({{"error", "title and author are required"}}, 400, sessions, sid, session);
    db.execute("UPDATE books SET title = ?, author = ?, status = ?, visibility = ? WHERE id = ?", {title, author, status, json_bool(payload, "visibility", true) ? "1" : "0", std::to_string(book_id)});
    return json_response({{"message", "book updated"}}, 200, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/issues").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto [user, err] = api_auth(db, sessions, session, sid); if (err) return *err;
    Rows rows = user->role == "admin"
      ? db.query("SELECT ib.id, ib.user_id, ib.book_id, ib.issue_date, ib.due_date, ib.return_date, ib.fine, u.username, b.title AS book_title FROM issued_books ib JOIN users u ON u.id = ib.user_id JOIN books b ON b.id = ib.book_id ORDER BY ib.id DESC")
      : db.query("SELECT ib.id, ib.user_id, ib.book_id, ib.issue_date, ib.due_date, ib.return_date, ib.fine, u.username, b.title AS book_title FROM issued_books ib JOIN users u ON u.id = ib.user_id JOIN books b ON b.id = ib.book_id WHERE ib.user_id = ? ORDER BY ib.id DESC", {std::to_string(user->id)});
    return json_response({{"issues", rows_json(rows)}}, 200, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/issues").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto [user, err] = api_auth(db, sessions, session, sid); if (err) return *err;
    json payload = json::parse(req.body.empty() ? "{}" : req.body, nullptr, false);
    if (payload.is_discarded()) payload = json::object();
    int issue_user_id = user->id;
    if (user->role == "admin") {
      if (payload.contains("user_id") && !payload["user_id"].is_null()) issue_user_id = payload["user_id"].get<int>();
      else if (payload.contains("username")) {
        auto rows = db.query("SELECT id FROM users WHERE username = ? AND role = 'student'", {json_string(payload, "username")});
        if (rows.empty()) return json_response({{"error", "student user not found"}}, 404, sessions, sid, session);
        issue_user_id = std::stoi(rows[0]["id"]);
      }
    }
    auto result = issue_book_for_user(db, issue_user_id, json_string(payload, "book_id"), parse_loan_days(json_string(payload, "loan_days"), db.config().default_loan_days), user->role == "student");
    if (!result.ok) return json_response({{"error", result.message}}, 400, sessions, sid, session);
    return json_response({{"message", result.message}}, 201, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/issues/<int>/return").methods(crow::HTTPMethod::POST)([&](const crow::request& req, int issue_id) {
    std::string sid; auto session = sessions.load(req, sid);
    auto [user, err] = api_auth(db, sessions, session, sid); if (err) return *err;
    auto rows = db.query("SELECT id, user_id FROM issued_books WHERE id = ?", {std::to_string(issue_id)});
    if (rows.empty()) return json_response({{"error", "issue record not found"}}, 404, sessions, sid, session);
    if (user->role == "student" && std::stoi(rows[0]["user_id"]) != user->id) return json_response({{"error", "forbidden"}}, 403, sessions, sid, session);
    auto result = return_issued_book(db, issue_id);
    if (!result.ok) return json_response({{"error", result.message}}, 400, sessions, sid, session);
    return json_response({{"message", result.message}, {"fine", result.fine}}, 200, sessions, sid, session);
  });

  CROW_ROUTE(app, "/api/me").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto [user, err] = api_auth(db, sessions, session, sid); if (err) return *err;
    return json_response({{"id", user->id}, {"username", user->username}, {"role", user->role}}, 200, sessions, sid, session);
  });
}

}  // namespace library
