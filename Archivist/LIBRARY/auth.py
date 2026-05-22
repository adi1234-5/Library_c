from functools import wraps

from flask import flash, jsonify, redirect, session, url_for
from werkzeug.security import check_password_hash, generate_password_hash

from database import get_db


def current_user():
    user_id = session.get("user_id")
    if not user_id:
        return None
    return get_db().execute(
        "SELECT id, username, role FROM users WHERE id = ?", (user_id,)
    ).fetchone()


def login_required(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        if current_user() is None:
            flash("Please login first.", "error")
            return redirect(url_for("login"))
        return view(*args, **kwargs)

    return wrapped


def role_required(*roles):
    def decorator(view):
        @wraps(view)
        def wrapped(*args, **kwargs):
            user = current_user()
            if user is None:
                flash("Please login first.", "error")
                return redirect(url_for("login"))
            if user["role"] not in roles:
                flash("You do not have permission to access this page.", "error")
                return redirect(url_for("home"))
            return view(*args, **kwargs)

        return wrapped

    return decorator


def api_auth_required(*roles):
    user = current_user()
    if user is None:
        return None, (jsonify({"error": "Authentication required"}), 401)
    if roles and user["role"] not in roles:
        return None, (jsonify({"error": "Forbidden"}), 403)
    return user, None
