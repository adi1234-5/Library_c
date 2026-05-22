from datetime import date, timedelta

from flask import current_app, flash, jsonify, redirect, render_template, request, session, url_for

from auth import api_auth_required, check_password_hash, current_user, generate_password_hash, login_required, role_required
from database import get_db


class RouteRegistry:
    def __init__(self):
        self._routes = []

    def route(self, rule, **options):
        def decorator(view):
            self._routes.append((rule, options, view))
            return view

        return decorator

    def register(self, app):
        for rule, options, view in self._routes:
            app.add_url_rule(rule, endpoint=view.__name__, view_func=view, **options)


bp = RouteRegistry()


def register_routes(app):
    bp.register(app)


def parse_loan_days(raw_value):
    try:
        loan_days = int(raw_value)
    except (TypeError, ValueError):
        return current_app.config["DEFAULT_LOAN_DAYS"]
    return loan_days if loan_days in (7, 14) else current_app.config["DEFAULT_LOAN_DAYS"]


def issue_book_for_user(user_id, book_id, loan_days, enforce_visibility=False):
    db = get_db()
    book = db.execute(
        "SELECT id, status, visibility, title FROM books WHERE id = ?",
        (book_id,),
    ).fetchone()
    if not book:
        return False, "Book not found."
    if enforce_visibility and int(book["visibility"]) != 1:
        return False, "This book is currently hidden from student access."
    if book["status"] != "Available":
        return False, "Book is not available for issue."

    issue_date = date.today()
    due_date = issue_date + timedelta(days=loan_days)
    db.execute(
        """
        INSERT INTO issued_books (user_id, book_id, issue_date, due_date, return_date, fine)
        VALUES (?, ?, ?, ?, NULL, 0)
        """,
        (user_id, book_id, issue_date.isoformat(), due_date.isoformat()),
    )
    db.execute("UPDATE books SET status = 'Issued' WHERE id = ?", (book_id,))
    db.commit()
    return True, f"Issued '{book['title']}' successfully."


def return_issued_book(issue_id):
    db = get_db()
    issue = db.execute(
        """
        SELECT ib.id, ib.book_id, ib.due_date, ib.return_date, b.title
        FROM issued_books ib
        JOIN books b ON b.id = ib.book_id
        WHERE ib.id = ?
        """,
        (issue_id,),
    ).fetchone()

    if not issue:
        return False, "Issue record not found.", 0
    if issue["return_date"] is not None:
        return False, "Book has already been returned.", 0

    today = date.today()
    due_date = date.fromisoformat(issue["due_date"])
    late_days = max((today - due_date).days, 0)
    fine = late_days * float(current_app.config["FINE_PER_DAY"])

    db.execute(
        "UPDATE issued_books SET return_date = ?, fine = ? WHERE id = ?",
        (today.isoformat(), fine, issue_id),
    )
    db.execute("UPDATE books SET status = 'Available' WHERE id = ?", (issue["book_id"],))
    db.commit()
    return True, f"Returned '{issue['title']}' successfully.", fine


@bp.route("/")
def home():
    return render_template("home.html")


@bp.route("/dashboard")
@login_required
def dashboard():
    user = current_user()
    if user["role"] == "admin":
        return redirect(url_for("admin_inventory"))
    return redirect(url_for("student_dashboard"))


@bp.route("/login", methods=["GET", "POST"])
def login():
    if request.method == "POST":
        username = request.form.get("username", "").strip()
        password = request.form.get("password", "")
        if not username or not password:
            flash("Username and password are required.", "error")
            return redirect(url_for("login"))

        user = get_db().execute(
            "SELECT id, username, password, role FROM users WHERE username = ?",
            (username,),
        ).fetchone()
        if not user or not check_password_hash(user["password"], password):
            flash("Invalid username or password.", "error")
            return redirect(url_for("login"))

        session.clear()
        session["user_id"] = user["id"]
        session["role"] = user["role"]
        flash(f"Welcome back, {user['username']}!", "success")
        return redirect(url_for("dashboard"))

    return render_template("login.html")


@bp.route("/signup", methods=["POST"])
def signup():
    username = request.form.get("username", "").strip()
    password = request.form.get("password", "")
    role = request.form.get("role", "student").strip().lower()
    if role not in ("admin", "student"):
        role = "student"
    if not username or not password:
        flash("Username and password are required.", "error")
        return redirect(url_for("login"))

    db = get_db()
    if db.execute("SELECT id FROM users WHERE username = ?", (username,)).fetchone():
        flash("Username already exists. Please choose another.", "error")
        return redirect(url_for("login"))

    db.execute(
        "INSERT INTO users (username, password, role) VALUES (?, ?, ?)",
        (username, generate_password_hash(password), role),
    )
    db.commit()
    flash("Signup successful. Please login.", "success")
    return redirect(url_for("login"))


@bp.route("/logout", methods=["POST"])
def logout():
    session.clear()
    flash("You have been logged out.", "success")
    return redirect(url_for("home"))


@bp.route("/admin/inventory")
@role_required("admin")
def admin_inventory():
    db = get_db()
    books = db.execute(
        "SELECT id, title, author, status, visibility FROM books ORDER BY id DESC"
    ).fetchall()
    issued_overview = db.execute(
        """
        SELECT ib.id, ib.issue_date, ib.due_date, ib.return_date, ib.fine,
               u.username AS student_name, b.title AS book_title
        FROM issued_books ib
        JOIN users u ON u.id = ib.user_id
        JOIN books b ON b.id = ib.book_id
        ORDER BY ib.issue_date DESC
        """
    ).fetchall()
    return render_template("admin_inventory.html", books=books, issued_overview=issued_overview)


@bp.route("/admin/books/add", methods=["POST"])
@role_required("admin")
def add_book():
    title = request.form.get("title", "").strip()
    author = request.form.get("author", "").strip()
    visibility = 1 if request.form.get("visibility", "1") == "1" else 0
    if not title or not author:
        flash("Title and author are required.", "error")
        return redirect(url_for("admin_inventory"))
    db = get_db()
    db.execute(
        "INSERT INTO books (title, author, status, visibility) VALUES (?, ?, 'Available', ?)",
        (title, author, visibility),
    )
    db.commit()
    flash("Book added successfully.", "success")
    return redirect(url_for("admin_inventory"))


@bp.route("/admin/books/<int:book_id>/edit", methods=["POST"])
@role_required("admin")
def edit_book(book_id):
    title = request.form.get("title", "").strip()
    author = request.form.get("author", "").strip()
    status = request.form.get("status", "Available").strip()
    visibility = 1 if request.form.get("visibility", "1") == "1" else 0
    if status not in ("Available", "Issued"):
        status = "Available"
    if not title or not author:
        flash("Title and author are required.", "error")
        return redirect(url_for("admin_inventory"))
    get_db().execute(
        "UPDATE books SET title = ?, author = ?, status = ?, visibility = ? WHERE id = ?",
        (title, author, status, visibility, book_id),
    )
    get_db().commit()
    flash("Book updated successfully.", "success")
    return redirect(url_for("admin_inventory"))


@bp.route("/admin/books/<int:book_id>/delete", methods=["POST"])
@role_required("admin")
def delete_book(book_id):
    db = get_db()
    active_issue = db.execute(
        "SELECT id FROM issued_books WHERE book_id = ? AND return_date IS NULL",
        (book_id,),
    ).fetchone()
    if active_issue:
        flash("Cannot delete a book that is currently issued.", "error")
        return redirect(url_for("admin_inventory"))
    db.execute("DELETE FROM books WHERE id = ?", (book_id,))
    db.commit()
    flash("Book deleted successfully.", "success")
    return redirect(url_for("admin_inventory"))


@bp.route("/admin/books/<int:book_id>/visibility", methods=["POST"])
@role_required("admin")
def toggle_book_visibility(book_id):
    visibility = 1 if request.form.get("visibility", "1") == "1" else 0
    db = get_db()
    db.execute("UPDATE books SET visibility = ? WHERE id = ?", (visibility, book_id))
    db.commit()
    flash("Book visibility updated.", "success")
    return redirect(url_for("admin_inventory"))


@bp.route("/admin/circulation")
@role_required("admin")
def admin_circulation():
    db = get_db()
    active_issues = db.execute(
        """
        SELECT ib.id, ib.issue_date, ib.due_date, ib.fine,
               u.username AS student_name, b.title AS book_title, b.id AS book_id
        FROM issued_books ib
        JOIN users u ON u.id = ib.user_id
        JOIN books b ON b.id = ib.book_id
        WHERE ib.return_date IS NULL
        ORDER BY ib.issue_date DESC
        """
    ).fetchall()
    transactions = db.execute(
        """
        SELECT ib.id, ib.issue_date, ib.due_date, ib.return_date, ib.fine,
               u.username AS student_name, b.title AS book_title
        FROM issued_books ib
        JOIN users u ON u.id = ib.user_id
        JOIN books b ON b.id = ib.book_id
        ORDER BY ib.id DESC
        LIMIT 50
        """
    ).fetchall()
    available_books = db.execute(
        "SELECT id, title, author FROM books WHERE status = 'Available' ORDER BY title"
    ).fetchall()
    students = db.execute(
        "SELECT id, username FROM users WHERE role = 'student' ORDER BY username"
    ).fetchall()
    return render_template(
        "admin_circulation.html",
        active_issues=active_issues,
        transactions=transactions,
        available_books=available_books,
        students=students,
    )


@bp.route("/admin/issue", methods=["POST"])
@role_required("admin")
def admin_issue_book():
    username = request.form.get("username", "").strip()
    book_id = request.form.get("book_id")
    loan_days = parse_loan_days(request.form.get("loan_days"))
    student = get_db().execute(
        "SELECT id, username FROM users WHERE username = ? AND role = 'student'",
        (username,),
    ).fetchone()
    if not student:
        flash("Student username not found.", "error")
        return redirect(url_for("admin_circulation"))
    success, message = issue_book_for_user(student["id"], book_id, loan_days)
    flash(message, "success" if success else "error")
    return redirect(url_for("admin_circulation"))


@bp.route("/admin/return/<int:issue_id>", methods=["POST"])
@role_required("admin")
def admin_return_book(issue_id):
    success, message, fine = return_issued_book(issue_id)
    if success and fine > 0:
        message = f"{message} Fine: Rs. {fine:.2f}"
    flash(message, "success" if success else "error")
    return redirect(url_for("admin_circulation"))


@bp.route("/books/search")
@login_required
def search_books():
    q = request.args.get("q", "").strip()
    user = current_user()
    sql = "SELECT id, title, author, status, visibility FROM books WHERE 1=1"
    params = []
    if user["role"] == "student":
        sql += " AND visibility = 1"
    if q:
        sql += " AND (LOWER(title) LIKE ? OR LOWER(author) LIKE ?)"
        like_query = f"%{q.lower()}%"
        params.extend([like_query, like_query])
    sql += " ORDER BY title ASC"
    books = get_db().execute(sql, params).fetchall()
    return render_template("search_books.html", books=books, q=q)


@bp.route("/student/dashboard")
@role_required("student")
def student_dashboard():
    user = current_user()
    db = get_db()
    today = date.today()
    active_issues = db.execute(
        """
        SELECT ib.id, ib.issue_date, ib.due_date, ib.fine, b.title, b.author, b.id AS book_id
        FROM issued_books ib
        JOIN books b ON b.id = ib.book_id
        WHERE ib.user_id = ? AND ib.return_date IS NULL
        ORDER BY ib.due_date ASC
        """,
        (user["id"],),
    ).fetchall()
    history = db.execute(
        """
        SELECT ib.id, ib.issue_date, ib.due_date, ib.return_date, ib.fine, b.title
        FROM issued_books ib
        JOIN books b ON b.id = ib.book_id
        WHERE ib.user_id = ? AND ib.return_date IS NOT NULL
        ORDER BY ib.return_date DESC
        LIMIT 10
        """,
        (user["id"],),
    ).fetchall()
    total_borrowed = db.execute(
        "SELECT COUNT(*) AS total FROM issued_books WHERE user_id = ?", (user["id"],)
    ).fetchone()["total"]
    due_soon = db.execute(
        """
        SELECT COUNT(*) AS total FROM issued_books
        WHERE user_id = ? AND return_date IS NULL AND due_date BETWEEN ? AND ?
        """,
        (user["id"], today.isoformat(), (today + timedelta(days=2)).isoformat()),
    ).fetchone()["total"]
    overdue = db.execute(
        """
        SELECT COUNT(*) AS total FROM issued_books
        WHERE user_id = ? AND return_date IS NULL AND due_date < ?
        """,
        (user["id"], today.isoformat()),
    ).fetchone()["total"]
    paid_or_recorded_fine = db.execute(
        "SELECT COALESCE(SUM(fine), 0) AS total FROM issued_books WHERE user_id = ?",
        (user["id"],),
    ).fetchone()["total"]
    dynamic_overdue_fine = 0.0
    for row in active_issues:
        due_date = date.fromisoformat(row["due_date"])
        dynamic_overdue_fine += max((today - due_date).days, 0) * float(current_app.config["FINE_PER_DAY"])
    total_fines = float(paid_or_recorded_fine) + dynamic_overdue_fine
    return render_template(
        "student_dashboard.html",
        active_issues=active_issues,
        history=history,
        total_borrowed=total_borrowed,
        due_soon=due_soon,
        overdue=overdue,
        total_fines=total_fines,
        today=today,
    )


@bp.route("/student/issue/<int:book_id>", methods=["POST"])
@role_required("student")
def student_issue_book(book_id):
    user = current_user()
    loan_days = parse_loan_days(request.form.get("loan_days"))
    success, message = issue_book_for_user(user["id"], book_id, loan_days, enforce_visibility=True)
    flash(message, "success" if success else "error")
    return redirect(request.referrer or url_for("search_books"))


@bp.route("/student/return/<int:issue_id>", methods=["POST"])
@role_required("student")
def student_return_book(issue_id):
    issue = get_db().execute(
        "SELECT id FROM issued_books WHERE id = ? AND user_id = ?",
        (issue_id, current_user()["id"]),
    ).fetchone()
    if not issue:
        flash("Issue record not found for your account.", "error")
        return redirect(url_for("student_dashboard"))
    success, message, fine = return_issued_book(issue_id)
    if success and fine > 0:
        message = f"{message} Fine: Rs. {fine:.2f}"
    flash(message, "success" if success else "error")
    return redirect(url_for("student_dashboard"))


@bp.route("/api/signup", methods=["POST"])
def api_signup():
    payload = request.get_json(silent=True) or {}
    username = str(payload.get("username", "")).strip()
    password = str(payload.get("password", ""))
    role = str(payload.get("role", "student")).strip().lower()
    if role not in ("admin", "student"):
        role = "student"
    if not username or not password:
        return jsonify({"error": "username and password are required"}), 400
    db = get_db()
    if db.execute("SELECT id FROM users WHERE username = ?", (username,)).fetchone():
        return jsonify({"error": "username already exists"}), 409
    db.execute(
        "INSERT INTO users (username, password, role) VALUES (?, ?, ?)",
        (username, generate_password_hash(password), role),
    )
    db.commit()
    return jsonify({"message": "signup successful", "username": username, "role": role}), 201


@bp.route("/api/login", methods=["POST"])
def api_login():
    payload = request.get_json(silent=True) or {}
    username = str(payload.get("username", "")).strip()
    password = str(payload.get("password", ""))
    if not username or not password:
        return jsonify({"error": "username and password are required"}), 400
    user = get_db().execute(
        "SELECT id, username, password, role FROM users WHERE username = ?", (username,)
    ).fetchone()
    if not user or not check_password_hash(user["password"], password):
        return jsonify({"error": "invalid username or password"}), 401
    session.clear()
    session["user_id"] = user["id"]
    session["role"] = user["role"]
    return jsonify({"message": "login successful", "user": {"id": user["id"], "username": user["username"], "role": user["role"]}})


@bp.route("/api/logout", methods=["POST"])
def api_logout():
    session.clear()
    return jsonify({"message": "logout successful"})


@bp.route("/api/books", methods=["GET", "POST"])
def api_books():
    user, error = api_auth_required()
    if error:
        return error
    db = get_db()
    if request.method == "GET":
        sql = "SELECT id, title, author, status, visibility FROM books"
        params = []
        query = request.args.get("q", "").strip().lower()
        if user["role"] == "student":
            sql += " WHERE visibility = 1"
            if query:
                sql += " AND (LOWER(title) LIKE ? OR LOWER(author) LIKE ?)"
                params.extend([f"%{query}%", f"%{query}%"])
        elif query:
            sql += " WHERE (LOWER(title) LIKE ? OR LOWER(author) LIKE ?)"
            params.extend([f"%{query}%", f"%{query}%"])
        sql += " ORDER BY title"
        return jsonify({"books": [dict(row) for row in db.execute(sql, params).fetchall()]})

    _, error = api_auth_required("admin")
    if error:
        return error
    payload = request.get_json(silent=True) or {}
    title = str(payload.get("title", "")).strip()
    author = str(payload.get("author", "")).strip()
    visibility = 1 if bool(payload.get("visibility", True)) else 0
    if not title or not author:
        return jsonify({"error": "title and author are required"}), 400
    cursor = db.execute(
        "INSERT INTO books (title, author, status, visibility) VALUES (?, ?, 'Available', ?)",
        (title, author, visibility),
    )
    db.commit()
    return jsonify({"message": "book created", "book_id": cursor.lastrowid}), 201


@bp.route("/api/books/<int:book_id>", methods=["PUT", "DELETE"])
def api_book_detail(book_id):
    _, error = api_auth_required("admin")
    if error:
        return error
    db = get_db()
    if request.method == "DELETE":
        active_issue = db.execute(
            "SELECT id FROM issued_books WHERE book_id = ? AND return_date IS NULL",
            (book_id,),
        ).fetchone()
        if active_issue:
            return jsonify({"error": "cannot delete currently issued book"}), 400
        db.execute("DELETE FROM books WHERE id = ?", (book_id,))
        db.commit()
        return jsonify({"message": "book deleted"})

    payload = request.get_json(silent=True) or {}
    title = str(payload.get("title", "")).strip()
    author = str(payload.get("author", "")).strip()
    status = str(payload.get("status", "Available")).strip()
    visibility = 1 if bool(payload.get("visibility", True)) else 0
    if status not in ("Available", "Issued"):
        return jsonify({"error": "invalid status"}), 400
    if not title or not author:
        return jsonify({"error": "title and author are required"}), 400
    db.execute(
        "UPDATE books SET title = ?, author = ?, status = ?, visibility = ? WHERE id = ?",
        (title, author, status, visibility, book_id),
    )
    db.commit()
    return jsonify({"message": "book updated"})


@bp.route("/api/issues", methods=["GET", "POST"])
def api_issues():
    user, error = api_auth_required()
    if error:
        return error
    db = get_db()
    if request.method == "GET":
        if user["role"] == "admin":
            rows = db.execute(
                """
                SELECT ib.id, ib.user_id, ib.book_id, ib.issue_date, ib.due_date, ib.return_date, ib.fine,
                       u.username, b.title AS book_title
                FROM issued_books ib
                JOIN users u ON u.id = ib.user_id
                JOIN books b ON b.id = ib.book_id
                ORDER BY ib.id DESC
                """
            ).fetchall()
        else:
            rows = db.execute(
                """
                SELECT ib.id, ib.user_id, ib.book_id, ib.issue_date, ib.due_date, ib.return_date, ib.fine,
                       u.username, b.title AS book_title
                FROM issued_books ib
                JOIN users u ON u.id = ib.user_id
                JOIN books b ON b.id = ib.book_id
                WHERE ib.user_id = ?
                ORDER BY ib.id DESC
                """,
                (user["id"],),
            ).fetchall()
        return jsonify({"issues": [dict(row) for row in rows]})

    payload = request.get_json(silent=True) or {}
    issue_user_id = user["id"]
    if user["role"] == "admin":
        if payload.get("user_id"):
            issue_user_id = int(payload["user_id"])
        elif payload.get("username"):
            student = db.execute(
                "SELECT id FROM users WHERE username = ? AND role = 'student'",
                (str(payload["username"]).strip(),),
            ).fetchone()
            if not student:
                return jsonify({"error": "student user not found"}), 404
            issue_user_id = student["id"]
    success, message = issue_book_for_user(
        issue_user_id,
        payload.get("book_id"),
        parse_loan_days(payload.get("loan_days")),
        enforce_visibility=user["role"] == "student",
    )
    if not success:
        return jsonify({"error": message}), 400
    return jsonify({"message": message}), 201


@bp.route("/api/issues/<int:issue_id>/return", methods=["POST"])
def api_return_issue(issue_id):
    user, error = api_auth_required()
    if error:
        return error
    issue = get_db().execute(
        "SELECT id, user_id FROM issued_books WHERE id = ?", (issue_id,)
    ).fetchone()
    if not issue:
        return jsonify({"error": "issue record not found"}), 404
    if user["role"] == "student" and issue["user_id"] != user["id"]:
        return jsonify({"error": "forbidden"}), 403
    success, message, fine = return_issued_book(issue_id)
    if not success:
        return jsonify({"error": message}), 400
    return jsonify({"message": message, "fine": fine})


@bp.route("/api/me")
def api_me():
    user, error = api_auth_required()
    if error:
        return error
    return jsonify({"id": user["id"], "username": user["username"], "role": user["role"]})
