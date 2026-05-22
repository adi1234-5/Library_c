#include "render.hpp"

#include "utils.hpp"

#include <algorithm>
#include <sstream>

namespace library {
namespace {

std::string value(const Row& row, const std::string& key) {
  auto it = row.find(key);
  return it == row.end() ? "" : it->second;
}

int int_value(const Row& row, const std::string& key) {
  try { return std::stoi(value(row, key)); } catch (...) { return 0; }
}

double double_value(const Row& row, const std::string& key) {
  try { return std::stod(value(row, key)); } catch (...) { return 0.0; }
}

void replace_all(std::string& text, const std::string& from, const std::string& to) {
  if (from.empty()) return;
  size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
}

void replace_block(std::string& text, const std::string& start_tag, const std::string& end_tag, const std::string& replacement) {
  size_t start = text.find(start_tag);
  if (start == std::string::npos) return;
  size_t end = text.find(end_tag, start + start_tag.size());
  if (end == std::string::npos) return;
  text.replace(start, end + end_tag.size() - start, replacement);
}

void replace_for_else_block(std::string& text, const std::string& start_tag, const std::string& replacement) {
  size_t start = text.find(start_tag);
  if (start == std::string::npos) return;
  size_t end = text.find("{% endfor %}", start + start_tag.size());
  if (end == std::string::npos) return;
  text.replace(start, end + std::string("{% endfor %}").size() - start, replacement);
}

void replace_urls(std::string& text) {
  replace_all(text, "{{ url_for('home') }}", route_url("home"));
  replace_all(text, "{{ url_for('dashboard') }}", route_url("dashboard"));
  replace_all(text, "{{ url_for('login') }}", route_url("login"));
  replace_all(text, "{{ url_for('signup') }}", route_url("signup"));
  replace_all(text, "{{ url_for('logout') }}", route_url("logout"));
  replace_all(text, "{{ url_for('admin_inventory') }}", route_url("admin_inventory"));
  replace_all(text, "{{ url_for('admin_circulation') }}", route_url("admin_circulation"));
  replace_all(text, "{{ url_for('add_book') }}", route_url("add_book"));
  replace_all(text, "{{ url_for('admin_issue_book') }}", route_url("admin_issue_book"));
  replace_all(text, "{{ url_for('search_books') }}", route_url("search_books"));
  replace_all(text, "{{ url_for('student_dashboard') }}", route_url("student_dashboard"));
}

std::string load_template(const AppConfig& config, const std::string& name, const std::vector<Flash>& flashes) {
  std::string html = read_text_file(config.templates_path / name);
  replace_all(html, "{% include \"_flash_messages.html\" %}", render_flash_messages(flashes));
  replace_urls(html);
  return html;
}

std::string row_text(const Row& row, const std::string& key) {
  return html_escape(value(row, key));
}

}  // namespace

std::string render_flash_messages(const std::vector<Flash>& flashes) {
  if (flashes.empty()) return "";
  std::ostringstream html;
  html << "<div class=\"space-y-2 mb-6\">";
  for (const auto& flash : flashes) {
    std::string color = "bg-blue-50 text-blue-800 border-blue-200";
    if (flash.category == "error") color = "bg-red-50 text-red-800 border-red-200";
    if (flash.category == "success") color = "bg-green-50 text-green-800 border-green-200";
    html << "<div class=\"px-4 py-3 rounded-lg border " << color << " text-sm font-medium\">"
         << html_escape(flash.message) << "</div>";
  }
  html << "</div>";
  return html.str();
}

std::string render_login(const AppConfig& config, const std::vector<Flash>& flashes) {
  return load_template(config, "login.html", flashes);
}

std::string render_home(const AppConfig& config, const std::optional<User>& user, const std::vector<Flash>& flashes) {
  std::string html = load_template(config, "home.html", flashes);
  std::string admin_nav =
      "  <a class=\"text-slate-500 dark:text-slate-400 font-medium border-b-2 border-transparent hover:border-blue-700 dark:hover:border-blue-200 hover:text-blue-700 dark:hover:text-blue-200 pb-1 transition-all duration-200 ease-in-out\" href=\"/admin/inventory\">Inventory</a>\n"
      "  <a class=\"text-slate-500 dark:text-slate-400 font-medium border-b-2 border-transparent hover:border-blue-700 dark:hover:border-blue-200 hover:text-blue-700 dark:hover:text-blue-200 pb-1 transition-all duration-200 ease-in-out\" href=\"/admin/circulation\">Circulation</a>\n";
  std::string student_nav =
      "  <a class=\"text-slate-500 dark:text-slate-400 font-medium border-b-2 border-transparent hover:border-blue-700 dark:hover:border-blue-200 hover:text-blue-700 dark:hover:text-blue-200 pb-1 transition-all duration-200 ease-in-out\" href=\"/books/search\">Search</a>\n"
      "  <a class=\"text-slate-500 dark:text-slate-400 font-medium border-b-2 border-transparent hover:border-blue-700 dark:hover:border-blue-200 hover:text-blue-700 dark:hover:text-blue-200 pb-1 transition-all duration-200 ease-in-out\" href=\"/student/dashboard\">Dashboard</a>\n";
  size_t nav_start = html.find("{% if current_user and current_user[\"role\"] == \"admin\" %}");
  size_t nav_elif = html.find("{% elif current_user %}", nav_start);
  size_t nav_end = html.find("{% endif %}", nav_start);
  if (nav_start != std::string::npos && nav_elif != std::string::npos && nav_end != std::string::npos) {
    std::string replacement = user ? (user->role == "admin" ? admin_nav : student_nav) : "";
    html.replace(nav_start, nav_end + 11 - nav_start, replacement);
  }
  size_t account_start = html.find("{% if current_user %}");
  size_t account_else = html.find("{% else %}", account_start);
  size_t account_end = html.find("{% endif %}", account_start);
  if (account_start != std::string::npos && account_else != std::string::npos && account_end != std::string::npos) {
    std::string replacement;
    if (user) {
      replacement =
          "  <a class=\"px-4 py-2 bg-surface-container-highest text-on-primary-fixed-variant rounded-lg font-semibold text-sm hover:bg-surface-container-high transition-colors\" href=\"/dashboard\">\n    " +
          html_escape(user->username) + " (" + title_case_role(user->role) +
          ")\n  </a>\n  <form action=\"/logout\" method=\"post\" class=\"m-0\">\n    <button class=\"px-4 py-2 bg-primary text-white rounded-lg font-semibold text-sm hover:opacity-90 transition-all duration-200 ease-in-out\" type=\"submit\">Logout</button>\n  </form>\n";
    } else {
      replacement = "  <a class=\"px-4 py-2 bg-primary text-white rounded-lg font-semibold text-sm hover:opacity-90 transition-all duration-200 ease-in-out\" href=\"/login\">Login / Signup</a>\n";
    }
    html.replace(account_start, account_end + 11 - account_start, replacement);
  }
  replace_all(html, "{{ url_for('search_books') if current_user else url_for('login') }}", user ? "/books/search" : "/login");
  size_t hero_start = html.find("{% if current_user %}");
  size_t hero_else = html.find("{% else %}", hero_start);
  size_t hero_end = html.find("{% endif %}", hero_start);
  if (hero_start != std::string::npos && hero_else != std::string::npos && hero_end != std::string::npos) {
    std::string replacement = user
      ? "<a href=\"/dashboard\" class=\"px-8 py-4 bg-surface-container-highest text-on-primary-fixed-variant rounded-xl font-semibold hover:bg-surface-container-high transition-all flex items-center justify-center\">\n                            Dashboard\n                        </a>"
      : "<a href=\"/login\" class=\"px-8 py-4 bg-surface-container-highest text-on-primary-fixed-variant rounded-xl font-semibold hover:bg-surface-container-high transition-all flex items-center justify-center\">\n                            Login\n                        </a>";
    html.replace(hero_start, hero_end + 11 - hero_start, replacement);
  }
  replace_all(html, "{{ current_user[\"username\"] }}", user ? html_escape(user->username) : "");
  replace_all(html, "{{ current_user[\"role\"]|title }}", user ? title_case_role(user->role) : "");
  return html;
}

std::string render_admin_inventory(const AppConfig& config, const User& user, const Rows& books, const Rows& issued_overview, const std::vector<Flash>& flashes) {
  std::string html = load_template(config, "admin_inventory.html", flashes);
  replace_all(html, "{{ current_user[\"username\"] }}", html_escape(user.username));
  int issued = 0, visible = 0;
  for (const auto& book : books) {
    if (value(book, "status") == "Issued") ++issued;
    if (int_value(book, "visibility") == 1) ++visible;
  }
  replace_all(html, "{{ books|length }}", std::to_string(books.size()));
  replace_all(html, "{{ books|selectattr('status', 'equalto', 'Issued')|list|length }}", std::to_string(issued));
  replace_all(html, "{{ books|selectattr('visibility', 'equalto', 1)|list|length }}", std::to_string(visible));

  std::ostringstream book_rows;
  if (books.empty()) {
    book_rows << "<tr class=\"bg-white\"><td class=\"px-4 py-6 text-slate-500\" colspan=\"5\">No books yet. Add your first book above.</td></tr>";
  } else {
    for (const auto& book : books) {
      int id = int_value(book, "id");
      bool available = value(book, "status") == "Available";
      bool is_visible = int_value(book, "visibility") == 1;
      book_rows << "<tr class=\"bg-white align-top\"><td class=\"px-4 py-4\"><input class=\"w-full bg-[#f2f3fc] rounded-md border-none focus:ring-0 text-sm\" form=\"edit-book-" << id << "\" name=\"title\" required type=\"text\" value=\"" << row_text(book, "title") << "\"></td>"
                << "<td class=\"px-4 py-4\"><input class=\"w-full bg-[#f2f3fc] rounded-md border-none focus:ring-0 text-sm\" form=\"edit-book-" << id << "\" name=\"author\" required type=\"text\" value=\"" << row_text(book, "author") << "\"></td>"
                << "<td class=\"px-4 py-4\"><select class=\"w-full bg-[#f2f3fc] rounded-md border-none focus:ring-0 text-sm\" form=\"edit-book-" << id << "\" name=\"status\"><option value=\"Available\" " << (available ? "selected" : "") << ">Available</option><option value=\"Issued\" " << (!available ? "selected" : "") << ">Issued</option></select></td>"
                << "<td class=\"px-4 py-4\"><select class=\"w-full bg-[#f2f3fc] rounded-md border-none focus:ring-0 text-sm\" form=\"edit-book-" << id << "\" name=\"visibility\"><option value=\"1\" " << (is_visible ? "selected" : "") << ">Visible</option><option value=\"0\" " << (!is_visible ? "selected" : "") << ">Hidden</option></select></td>"
                << "<td class=\"px-4 py-4\"><div class=\"flex justify-end gap-2 flex-wrap\"><form action=\"" << route_url("edit_book", id) << "\" id=\"edit-book-" << id << "\" method=\"post\"><button class=\"px-3 py-2 bg-blue-50 text-blue-900 rounded-lg text-xs font-bold\" type=\"submit\">Save</button></form>"
                << "<form action=\"" << route_url("toggle_book_visibility", id) << "\" method=\"post\"><input name=\"visibility\" type=\"hidden\" value=\"" << (is_visible ? 0 : 1) << "\"><button class=\"px-3 py-2 bg-amber-50 text-amber-700 rounded-lg text-xs font-bold\" type=\"submit\">" << (is_visible ? "Hide" : "Show") << "</button></form>"
                << "<form action=\"" << route_url("delete_book", id) << "\" method=\"post\"><button class=\"px-3 py-2 bg-red-50 text-red-700 rounded-lg text-xs font-bold\" onclick=\"return confirm('Delete this book?')\" type=\"submit\">Delete</button></form></div></td></tr>";
    }
  }
  replace_for_else_block(html, "{% for book in books %}", book_rows.str());

  std::ostringstream issue_rows;
  if (issued_overview.empty()) {
    issue_rows << "<tr><td class=\"px-4 py-6 text-slate-500\" colspan=\"6\">No issue records yet.</td></tr>";
  } else {
    for (const auto& row : issued_overview) {
      issue_rows << "<tr><td class=\"px-4 py-3 font-semibold\">" << row_text(row, "book_title") << "</td><td class=\"px-4 py-3\">" << row_text(row, "student_name")
                 << "</td><td class=\"px-4 py-3\">" << row_text(row, "issue_date") << "</td><td class=\"px-4 py-3\">" << row_text(row, "due_date")
                 << "</td><td class=\"px-4 py-3\">" << (value(row, "return_date").empty() ? "Not Returned" : row_text(row, "return_date"))
                 << "</td><td class=\"px-4 py-3\">Rs. " << format_money(double_value(row, "fine")) << "</td></tr>";
    }
  }
  replace_for_else_block(html, "{% for row in issued_overview %}", issue_rows.str());
  return html;
}

std::string render_admin_circulation(const AppConfig& config, const User& user, const Rows& active_issues, const Rows& transactions, const Rows& available_books, const Rows& students, const std::vector<Flash>& flashes) {
  std::string html = load_template(config, "admin_circulation.html", flashes);
  replace_all(html, "{{ current_user[\"username\"] }}", html_escape(user.username));
  replace_all(html, "{{ active_issues|length }}", std::to_string(active_issues.size()));
  std::ostringstream student_options;
  for (const auto& s : students) student_options << "<option value=\"" << row_text(s, "username") << "\">" << row_text(s, "username") << "</option>";
  replace_for_else_block(html, "{% for student in students %}", student_options.str());
  std::ostringstream book_options;
  for (const auto& b : available_books) book_options << "<option value=\"" << row_text(b, "id") << "\">" << row_text(b, "title") << " - " << row_text(b, "author") << "</option>";
  replace_for_else_block(html, "{% for book in available_books %}", book_options.str());
  std::ostringstream active;
  if (active_issues.empty()) {
    active << "<p class=\"text-sm text-slate-500\">No active issues right now.</p>";
  } else {
    for (const auto& issue : active_issues) {
      int id = int_value(issue, "id");
      active << "<div class=\"bg-white rounded-lg p-4 border border-slate-200/60\"><div class=\"text-sm font-bold\">" << row_text(issue, "book_title")
             << "</div><div class=\"text-xs text-slate-600\">Student: " << row_text(issue, "student_name") << "</div><div class=\"text-xs text-slate-600\">Due: "
             << row_text(issue, "due_date") << "</div><form action=\"" << route_url("admin_return_book", id) << "\" class=\"mt-3\" method=\"post\"><button class=\"w-full py-2 bg-emerald-600 text-white text-xs font-bold rounded-md\" type=\"submit\">Confirm Return</button></form></div>";
    }
  }
  replace_for_else_block(html, "{% for issue in active_issues %}", active.str());
  std::ostringstream tx;
  if (transactions.empty()) {
    tx << "<tr><td class=\"px-6 py-6 text-slate-500\" colspan=\"6\">No transactions yet.</td></tr>";
  } else {
    for (const auto& row : transactions) {
      tx << "<tr><td class=\"px-6 py-4 text-sm font-semibold\">" << row_text(row, "book_title") << "</td><td class=\"px-6 py-4 text-sm\">" << row_text(row, "student_name")
         << "</td><td class=\"px-6 py-4 text-xs text-slate-600\">" << row_text(row, "issue_date") << "</td><td class=\"px-6 py-4 text-xs text-slate-600\">" << row_text(row, "due_date")
         << "</td><td class=\"px-6 py-4 text-xs text-slate-600\">" << (value(row, "return_date").empty() ? "Pending" : row_text(row, "return_date"))
         << "</td><td class=\"px-6 py-4 text-xs font-semibold\">Rs. " << format_money(double_value(row, "fine")) << "</td></tr>";
    }
  }
  replace_for_else_block(html, "{% for row in transactions %}", tx.str());
  return html;
}

std::string render_search_books(const AppConfig& config, const User& user, const Rows& books, const std::string& q, const std::vector<Flash>& flashes) {
  std::string html = load_template(config, "search_books.html", flashes);
  replace_all(html, "{{ current_user[\"username\"] }}", html_escape(user.username));
  replace_all(html, "{{ q }}", html_escape(q));
  replace_all(html, "{{ books|length }}", std::to_string(books.size()));
  std::string nav = user.role == "admin"
    ? "<a class=\"text-slate-500 font-medium border-b-2 border-transparent hover:border-blue-700 hover:text-blue-700 transition-all duration-200 ease-in-out pb-1\" href=\"/admin/inventory\">Inventory</a><a class=\"text-slate-500 font-medium border-b-2 border-transparent hover:border-blue-700 hover:text-blue-700 transition-all duration-200 ease-in-out pb-1\" href=\"/admin/circulation\">Circulation</a>"
    : "<a class=\"text-slate-500 font-medium border-b-2 border-transparent hover:border-blue-700 hover:text-blue-700 transition-all duration-200 ease-in-out pb-1\" href=\"/student/dashboard\">Dashboard</a>";
  replace_block(html, "{% if current_user[\"role\"] == \"admin\" %}", "{% endif %}", nav);
  replace_block(html, "{% if current_user[\"role\"] == \"student\" %}", "{% endif %}", user.role == "student" ? "<p class=\"text-sm text-slate-600\">Only books marked visible by admin are shown.</p>" : "");
  std::ostringstream cards;
  if (books.empty()) {
    cards << "<div class=\"col-span-full bg-white rounded-xl p-8 text-slate-500\">No matching books found.</div>";
  } else {
    for (const auto& book : books) {
      int id = int_value(book, "id");
      bool available = value(book, "status") == "Available";
      bool visible = int_value(book, "visibility") == 1;
      cards << "<div class=\"group bg-[#f2f3fc] rounded-xl overflow-hidden hover:shadow-[0_12px_40px_rgba(25,28,33,0.08)] transition-all duration-300\"><div class=\"aspect-[3/2] overflow-hidden bg-[#e1e2ea] relative p-6 flex flex-col justify-between\"><div class=\"absolute top-4 right-4 px-3 py-1 text-[10px] font-extrabold uppercase tracking-widest rounded-full "
            << (available ? "bg-emerald-50 text-emerald-800" : "bg-amber-50 text-amber-800") << "\">" << row_text(book, "status")
            << "</div><span class=\"material-symbols-outlined text-5xl text-slate-400\">auto_stories</span><p class=\"text-xs uppercase font-bold tracking-widest text-slate-500\">" << (visible ? "Visible" : "Hidden") << "</p></div><div class=\"p-6\"><h3 class=\"text-lg font-bold leading-tight\">" << row_text(book, "title") << "</h3><p class=\"text-slate-600 italic text-sm mb-4\">by " << row_text(book, "author") << "</p><div class=\"flex justify-between items-center border-t border-slate-200 pt-4 gap-2\"><span class=\"text-xs font-mono text-slate-500\">ID " << id << "</span>";
      if (user.role == "student") {
        cards << "<form action=\"" << route_url("student_issue_book", id) << "\" class=\"flex items-center gap-2\" method=\"post\"><select class=\"text-xs rounded-md border-slate-200 py-1\" name=\"loan_days\"><option value=\"7\">7d</option><option selected value=\"14\">14d</option></select><button class=\"px-3 py-2 rounded-full text-xs font-bold " << (available ? "bg-[#003f87] text-white" : "bg-slate-200 text-slate-500 cursor-not-allowed") << "\" " << (available ? "" : "disabled") << " type=\"submit\">Issue</button></form>";
      } else {
        cards << "<a class=\"px-3 py-2 rounded-full text-xs font-bold bg-blue-50 text-blue-900\" href=\"/admin/inventory\">Manage</a>";
      }
      cards << "</div></div></div>";
    }
  }
  replace_for_else_block(html, "{% for book in books %}", cards.str());
  return html;
}

std::string render_student_dashboard(const AppConfig& config, const User& user, const Rows& active_issues, const Rows& history, int total_borrowed, int due_soon, int overdue, double total_fines, const std::string& today, const std::vector<Flash>& flashes) {
  std::string html = load_template(config, "student_dashboard.html", flashes);
  replace_all(html, "{{ current_user[\"username\"] }}", html_escape(user.username));
  replace_all(html, "{{ total_borrowed }}", std::to_string(total_borrowed));
  replace_all(html, "{{ due_soon }}", std::to_string(due_soon));
  replace_all(html, "{{ overdue }}", std::to_string(overdue));
  replace_all(html, "{{ \"%.2f\"|format(total_fines) }}", format_money(total_fines));
  std::ostringstream active;
  if (active_issues.empty()) {
    active << "<div class=\"bg-white p-6 rounded-xl text-slate-500\">You have no active issued books.</div>";
  } else {
    for (const auto& issue : active_issues) {
      bool is_overdue = value(issue, "due_date") < today;
      active << "<div class=\"flex items-center gap-6 p-5 bg-white rounded-xl shadow-sm\"><div class=\"w-14 h-20 rounded-lg bg-[#e1e2ea] flex-shrink-0 flex items-center justify-center\"><span class=\"material-symbols-outlined text-slate-500\">book_2</span></div><div class=\"flex-grow space-y-1\"><div class=\"flex justify-between items-start gap-4\"><h4 class=\"font-bold text-lg leading-tight\">" << row_text(issue, "title") << "</h4><span class=\"px-3 py-1 text-[10px] font-bold rounded-full uppercase tracking-tighter "
             << (is_overdue ? "bg-red-100 text-red-700" : "bg-blue-100 text-blue-700") << "\">" << (is_overdue ? "Overdue" : "Active") << "</span></div><p class=\"text-slate-600 text-sm font-medium\">" << row_text(issue, "author") << "</p><div class=\"pt-2 flex items-center gap-8\"><div class=\"flex flex-col\"><span class=\"text-[10px] uppercase tracking-wider text-slate-500\">Issue Date</span><span class=\"text-sm font-semibold\">" << row_text(issue, "issue_date") << "</span></div><div class=\"flex flex-col\"><span class=\"text-[10px] uppercase tracking-wider text-slate-500\">Due Date</span><span class=\"text-sm font-semibold " << (is_overdue ? "text-red-600" : "") << "\">" << row_text(issue, "due_date") << "</span></div></div></div><form action=\"" << route_url("student_return_book", int_value(issue, "id")) << "\" method=\"post\"><button class=\"bg-[#003f87] text-white px-5 py-2 rounded-full text-xs font-bold\" type=\"submit\">Return</button></form></div>";
    }
  }
  replace_for_else_block(html, "{% for issue in active_issues %}", active.str());
  std::ostringstream hist;
  if (history.empty()) {
    hist << "<tr><td class=\"p-3 text-slate-500\" colspan=\"2\">No return history yet.</td></tr>";
  } else {
    for (const auto& item : history) {
      hist << "<tr class=\"hover:bg-white transition-colors\"><td class=\"p-3\"><p class=\"font-bold\">" << row_text(item, "title") << "</p><p class=\"text-xs text-slate-600\">Fine: Rs. " << format_money(double_value(item, "fine")) << "</p></td><td class=\"p-3 text-right text-slate-600 font-medium\">" << row_text(item, "return_date") << "</td></tr>";
    }
  }
  replace_for_else_block(html, "{% for item in history %}", hist.str());
  return html;
}

}  // namespace library
