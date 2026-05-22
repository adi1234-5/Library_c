#include "handlers.hpp"

#include "auth.hpp"
#include "render.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cctype>

namespace library {
namespace {

crow::response redirect_to(const std::string& url, SessionStore& sessions, const std::string& sid, const SessionData& session) {
  crow::response res(302);
  res.add_header("Location", url);
  sessions.save(sid, session);
  sessions.attach_cookie(res, sid);
  return res;
}

crow::response html_response(const std::string& body, SessionStore& sessions, const std::string& sid, const SessionData& session) {
  crow::response res(body);
  res.set_header("Content-Type", "text/html; charset=utf-8");
  sessions.save(sid, session);
  sessions.attach_cookie(res, sid);
  return res;
}

std::optional<User> require_login(Database& db, SessionStore& sessions, SessionData& session, crow::response& res, const std::string& sid) {
  auto user = current_user(db, session);
  if (!user) {
    sessions.flash(session, "Please login first.", "error");
    res = redirect_to("/login", sessions, sid, session);
  }
  return user;
}

std::optional<User> require_role(Database& db, SessionStore& sessions, SessionData& session, crow::response& res, const std::string& sid, const std::string& role) {
  auto user = require_login(db, sessions, session, res, sid);
  if (!user) return std::nullopt;
  if (user->role != role) {
    sessions.flash(session, "You do not have permission to access this page.", "error");
    res = redirect_to("/", sessions, sid, session);
    return std::nullopt;
  }
  return user;
}

std::string row_value_or(const Row& row, const std::string& key, const std::string& fallback) {
  auto it = row.find(key);
  return it == row.end() ? fallback : it->second;
}

}  // namespace

IssueResult issue_book_for_user(Database& db, int user_id, const std::string& raw_book_id, int loan_days, bool enforce_visibility) {
  int book_id = 0;
  try { book_id = std::stoi(raw_book_id); } catch (...) { return {false, "Book not found.", 0.0}; }
  auto rows = db.query("SELECT id, status, visibility, title FROM books WHERE id = ?", {std::to_string(book_id)});
  if (rows.empty()) return {false, "Book not found.", 0.0};
  const auto& book = rows.front();
  if (enforce_visibility && row_value_or(book, "visibility", "0") != "1") return {false, "This book is currently hidden from student access.", 0.0};
  if (book.at("status") != "Available") return {false, "Book is not available for issue.", 0.0};
  std::string today = today_iso();
  std::string due = add_days_iso(today, loan_days);
  db.execute("INSERT INTO issued_books (user_id, book_id, issue_date, due_date, return_date, fine) VALUES (?, ?, ?, ?, NULL, 0)",
             {std::to_string(user_id), std::to_string(book_id), today, due});
  db.execute("UPDATE books SET status = 'Issued' WHERE id = ?", {std::to_string(book_id)});
  return {true, "Issued '" + book.at("title") + "' successfully.", 0.0};
}

IssueResult return_issued_book(Database& db, int issue_id) {
  auto rows = db.query("SELECT ib.id, ib.book_id, ib.due_date, ib.return_date, b.title FROM issued_books ib JOIN books b ON b.id = ib.book_id WHERE ib.id = ?", {std::to_string(issue_id)});
  if (rows.empty()) return {false, "Issue record not found.", 0.0};
  const auto& issue = rows.front();
  if (!issue.at("return_date").empty()) return {false, "Book has already been returned.", 0.0};
  std::string today = today_iso();
  int late_days = std::max(days_between(today, issue.at("due_date")), 0);
  double fine = late_days * db.config().fine_per_day;
  db.execute("UPDATE issued_books SET return_date = ?, fine = ? WHERE id = ?", {today, std::to_string(fine), std::to_string(issue_id)});
  db.execute("UPDATE books SET status = 'Available' WHERE id = ?", {issue.at("book_id")});
  return {true, "Returned '" + issue.at("title") + "' successfully.", fine};
}

void register_page_routes(crow::SimpleApp& app, Database& db, SessionStore& sessions) {
  CROW_ROUTE(app, "/").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto user = current_user(db, session);
    auto flashes = sessions.consume_flashes(session);
    return html_response(render_home(db.config(), user, flashes), sessions, sid, session);
  });

  CROW_ROUTE(app, "/dashboard").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    crow::response res; auto user = require_login(db, sessions, session, res, sid);
    if (!user) return res;
    return redirect_to(user->role == "admin" ? "/admin/inventory" : "/student/dashboard", sessions, sid, session);
  });

  CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto flashes = sessions.consume_flashes(session);
    return html_response(render_login(db.config(), flashes), sessions, sid, session);
  });

  CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto form = parse_form_urlencoded(req.body);
    std::string username = get_form(form, "username");
    std::string password = get_form(form, "password");
    if (username.empty() || password.empty()) {
      sessions.flash(session, "Username and password are required.", "error");
      return redirect_to("/login", sessions, sid, session);
    }
    auto rows = db.query("SELECT id, username, password, role FROM users WHERE username = ?", {username});
    if (rows.empty() || !check_password_hash(rows[0]["password"], password)) {
      sessions.flash(session, "Invalid username or password.", "error");
      return redirect_to("/login", sessions, sid, session);
    }
    session = SessionData{};
    session.user_id = std::stoi(rows[0]["id"]);
    session.role = rows[0]["role"];
    sessions.flash(session, "Welcome back, " + rows[0]["username"] + "!", "success");
    return redirect_to("/dashboard", sessions, sid, session);
  });

  CROW_ROUTE(app, "/signup").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    auto form = parse_form_urlencoded(req.body);
    std::string username = get_form(form, "username");
    std::string password = get_form(form, "password");
    std::string role = get_form(form, "role", "student");
    if (role != "admin" && role != "student") role = "student";
    if (username.empty() || password.empty()) {
      sessions.flash(session, "Username and password are required.", "error");
      return redirect_to("/login", sessions, sid, session);
    }
    if (!db.query("SELECT id FROM users WHERE username = ?", {username}).empty()) {
      sessions.flash(session, "Username already exists. Please choose another.", "error");
      return redirect_to("/login", sessions, sid, session);
    }
    db.execute("INSERT INTO users (username, password, role) VALUES (?, ?, ?)", {username, generate_password_hash(password), role});
    sessions.flash(session, "Signup successful. Please login.", "success");
    return redirect_to("/login", sessions, sid, session);
  });

  CROW_ROUTE(app, "/logout").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid);
    session = SessionData{};
    sessions.flash(session, "You have been logged out.", "success");
    return redirect_to("/", sessions, sid, session);
  });

  CROW_ROUTE(app, "/admin/inventory").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "admin"); if (!user) return res;
    auto flashes = sessions.consume_flashes(session);
    auto books = db.query("SELECT id, title, author, status, visibility FROM books ORDER BY id DESC");
    auto overview = db.query("SELECT ib.id, ib.issue_date, ib.due_date, ib.return_date, ib.fine, u.username AS student_name, b.title AS book_title FROM issued_books ib JOIN users u ON u.id = ib.user_id JOIN books b ON b.id = ib.book_id ORDER BY ib.issue_date DESC");
    return html_response(render_admin_inventory(db.config(), *user, books, overview, flashes), sessions, sid, session);
  });

  CROW_ROUTE(app, "/admin/books/add").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "admin"); if (!user) return res;
    auto form = parse_form_urlencoded(req.body);
    std::string title = get_form(form, "title"), author = get_form(form, "author");
    std::string visibility = get_form(form, "visibility", "1") == "1" ? "1" : "0";
    if (title.empty() || author.empty()) sessions.flash(session, "Title and author are required.", "error");
    else { db.execute("INSERT INTO books (title, author, status, visibility) VALUES (?, ?, 'Available', ?)", {title, author, visibility}); sessions.flash(session, "Book added successfully.", "success"); }
    return redirect_to("/admin/inventory", sessions, sid, session);
  });

  CROW_ROUTE(app, "/admin/books/<int>/edit").methods(crow::HTTPMethod::POST)([&](const crow::request& req, int book_id) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "admin"); if (!user) return res;
    auto form = parse_form_urlencoded(req.body);
    std::string title = get_form(form, "title"), author = get_form(form, "author"), status = get_form(form, "status", "Available");
    if (status != "Available" && status != "Issued") status = "Available";
    std::string visibility = get_form(form, "visibility", "1") == "1" ? "1" : "0";
    if (title.empty() || author.empty()) sessions.flash(session, "Title and author are required.", "error");
    else { db.execute("UPDATE books SET title = ?, author = ?, status = ?, visibility = ? WHERE id = ?", {title, author, status, visibility, std::to_string(book_id)}); sessions.flash(session, "Book updated successfully.", "success"); }
    return redirect_to("/admin/inventory", sessions, sid, session);
  });

  CROW_ROUTE(app, "/admin/books/<int>/delete").methods(crow::HTTPMethod::POST)([&](const crow::request& req, int book_id) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "admin"); if (!user) return res;
    if (!db.query("SELECT id FROM issued_books WHERE book_id = ? AND return_date IS NULL", {std::to_string(book_id)}).empty()) sessions.flash(session, "Cannot delete a book that is currently issued.", "error");
    else { db.execute("DELETE FROM books WHERE id = ?", {std::to_string(book_id)}); sessions.flash(session, "Book deleted successfully.", "success"); }
    return redirect_to("/admin/inventory", sessions, sid, session);
  });

  CROW_ROUTE(app, "/admin/books/<int>/visibility").methods(crow::HTTPMethod::POST)([&](const crow::request& req, int book_id) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "admin"); if (!user) return res;
    auto form = parse_form_urlencoded(req.body);
    db.execute("UPDATE books SET visibility = ? WHERE id = ?", {get_form(form, "visibility", "1") == "1" ? "1" : "0", std::to_string(book_id)});
    sessions.flash(session, "Book visibility updated.", "success");
    return redirect_to("/admin/inventory", sessions, sid, session);
  });

  CROW_ROUTE(app, "/admin/circulation").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "admin"); if (!user) return res;
    auto flashes = sessions.consume_flashes(session);
    auto active = db.query("SELECT ib.id, ib.issue_date, ib.due_date, ib.fine, u.username AS student_name, b.title AS book_title, b.id AS book_id FROM issued_books ib JOIN users u ON u.id = ib.user_id JOIN books b ON b.id = ib.book_id WHERE ib.return_date IS NULL ORDER BY ib.issue_date DESC");
    auto tx = db.query("SELECT ib.id, ib.issue_date, ib.due_date, ib.return_date, ib.fine, u.username AS student_name, b.title AS book_title FROM issued_books ib JOIN users u ON u.id = ib.user_id JOIN books b ON b.id = ib.book_id ORDER BY ib.id DESC LIMIT 50");
    auto available = db.query("SELECT id, title, author FROM books WHERE status = 'Available' ORDER BY title");
    auto students = db.query("SELECT id, username FROM users WHERE role = 'student' ORDER BY username");
    return html_response(render_admin_circulation(db.config(), *user, active, tx, available, students, flashes), sessions, sid, session);
  });

  CROW_ROUTE(app, "/admin/issue").methods(crow::HTTPMethod::POST)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "admin"); if (!user) return res;
    auto form = parse_form_urlencoded(req.body);
    auto students = db.query("SELECT id, username FROM users WHERE username = ? AND role = 'student'", {get_form(form, "username")});
    if (students.empty()) sessions.flash(session, "Student username not found.", "error");
    else {
      auto result = issue_book_for_user(db, std::stoi(students[0]["id"]), get_form(form, "book_id"), parse_loan_days(get_form(form, "loan_days"), db.config().default_loan_days), false);
      sessions.flash(session, result.message, result.ok ? "success" : "error");
    }
    return redirect_to("/admin/circulation", sessions, sid, session);
  });

  CROW_ROUTE(app, "/admin/return/<int>").methods(crow::HTTPMethod::POST)([&](const crow::request& req, int issue_id) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "admin"); if (!user) return res;
    auto result = return_issued_book(db, issue_id);
    if (result.ok && result.fine > 0) result.message += " Fine: Rs. " + format_money(result.fine);
    sessions.flash(session, result.message, result.ok ? "success" : "error");
    return redirect_to("/admin/circulation", sessions, sid, session);
  });

  CROW_ROUTE(app, "/books/search").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_login(db, sessions, session, res, sid); if (!user) return res;
    auto flashes = sessions.consume_flashes(session);
    std::string q = query_param(req.raw_url, "q");
    std::string sql = "SELECT id, title, author, status, visibility FROM books WHERE 1=1";
    std::vector<std::string> params;
    if (user->role == "student") sql += " AND visibility = 1";
    if (!q.empty()) { sql += " AND (LOWER(title) LIKE ? OR LOWER(author) LIKE ?)"; std::string like = "%" + q + "%"; std::transform(like.begin(), like.end(), like.begin(), ::tolower); params = {like, like}; }
    sql += " ORDER BY title ASC";
    auto books = db.query(sql, params);
    return html_response(render_search_books(db.config(), *user, books, q, flashes), sessions, sid, session);
  });

  CROW_ROUTE(app, "/student/dashboard").methods(crow::HTTPMethod::GET)([&](const crow::request& req) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "student"); if (!user) return res;
    auto flashes = sessions.consume_flashes(session);
    std::string today = today_iso();
    auto active = db.query("SELECT ib.id, ib.issue_date, ib.due_date, ib.fine, b.title, b.author, b.id AS book_id FROM issued_books ib JOIN books b ON b.id = ib.book_id WHERE ib.user_id = ? AND ib.return_date IS NULL ORDER BY ib.due_date ASC", {std::to_string(user->id)});
    auto history = db.query("SELECT ib.id, ib.issue_date, ib.due_date, ib.return_date, ib.fine, b.title FROM issued_books ib JOIN books b ON b.id = ib.book_id WHERE ib.user_id = ? AND ib.return_date IS NOT NULL ORDER BY ib.return_date DESC LIMIT 10", {std::to_string(user->id)});
    int total = std::stoi(db.query("SELECT COUNT(*) AS total FROM issued_books WHERE user_id = ?", {std::to_string(user->id)})[0]["total"]);
    int due_soon = std::stoi(db.query("SELECT COUNT(*) AS total FROM issued_books WHERE user_id = ? AND return_date IS NULL AND due_date BETWEEN ? AND ?", {std::to_string(user->id), today, add_days_iso(today, 2)})[0]["total"]);
    int overdue = std::stoi(db.query("SELECT COUNT(*) AS total FROM issued_books WHERE user_id = ? AND return_date IS NULL AND due_date < ?", {std::to_string(user->id), today})[0]["total"]);
    double fines = std::stod(db.query("SELECT COALESCE(SUM(fine), 0) AS total FROM issued_books WHERE user_id = ?", {std::to_string(user->id)})[0]["total"]);
    for (const auto& row : active) fines += std::max(days_between(today, row.at("due_date")), 0) * db.config().fine_per_day;
    return html_response(render_student_dashboard(db.config(), *user, active, history, total, due_soon, overdue, fines, today, flashes), sessions, sid, session);
  });

  CROW_ROUTE(app, "/student/issue/<int>").methods(crow::HTTPMethod::POST)([&](const crow::request& req, int book_id) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "student"); if (!user) return res;
    auto form = parse_form_urlencoded(req.body);
    auto result = issue_book_for_user(db, user->id, std::to_string(book_id), parse_loan_days(get_form(form, "loan_days"), db.config().default_loan_days), true);
    sessions.flash(session, result.message, result.ok ? "success" : "error");
    std::string ref = req.get_header_value("Referer");
    return redirect_to(ref.empty() ? "/books/search" : ref, sessions, sid, session);
  });

  CROW_ROUTE(app, "/student/return/<int>").methods(crow::HTTPMethod::POST)([&](const crow::request& req, int issue_id) {
    std::string sid; auto session = sessions.load(req, sid); crow::response res;
    auto user = require_role(db, sessions, session, res, sid, "student"); if (!user) return res;
    if (db.query("SELECT id FROM issued_books WHERE id = ? AND user_id = ?", {std::to_string(issue_id), std::to_string(user->id)}).empty()) {
      sessions.flash(session, "Issue record not found for your account.", "error");
    } else {
      auto result = return_issued_book(db, issue_id);
      if (result.ok && result.fine > 0) result.message += " Fine: Rs. " + format_money(result.fine);
      sessions.flash(session, result.message, result.ok ? "success" : "error");
    }
    return redirect_to("/student/dashboard", sessions, sid, session);
  });
}

}  // namespace library
