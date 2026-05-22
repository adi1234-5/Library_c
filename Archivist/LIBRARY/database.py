import sqlite3

from flask import current_app, g


def get_db():
    if "db" not in g:
        g.db = sqlite3.connect(current_app.config["DATABASE"])
        g.db.row_factory = sqlite3.Row
        g.db.execute("PRAGMA foreign_keys = ON")
    return g.db


def close_db(_error=None):
    db = g.pop("db", None)
    if db is not None:
        db.close()


def init_db():
    db = get_db()
    with open(current_app.config["SCHEMA"], "r", encoding="utf-8") as schema_file:
        db.executescript(schema_file.read())

    seed_user("admin", "admin123", "admin")
    seed_user("student", "student123", "student")
    db.commit()


def ensure_database():
    db = get_db()
    db.execute(
        """
        CREATE TABLE IF NOT EXISTS users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password TEXT NOT NULL,
            role TEXT NOT NULL CHECK (role IN ('admin', 'student'))
        )
        """
    )
    existing_tables = db.execute(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='books'"
    ).fetchone()
    if existing_tables is None:
        init_db()
    else:
        seed_user("admin", "admin123", "admin")
        seed_user("student", "student123", "student")
        db.commit()


def seed_user(username, password, role):
    from auth import generate_password_hash

    db = get_db()
    existing = db.execute("SELECT id FROM users WHERE username = ?", (username,)).fetchone()
    if existing:
        return
    db.execute(
        "INSERT INTO users (username, password, role) VALUES (?, ?, ?)",
        (username, generate_password_hash(password), role),
    )
