import os
from pathlib import Path


BASE_DIR = Path(__file__).resolve().parent


class Config:
    SECRET_KEY = os.environ.get("SECRET_KEY", "change-me-in-production")
    DATABASE = os.environ.get("LIBRARY_DB_PATH", str(BASE_DIR / "database" / "library.db"))
    SCHEMA = os.environ.get("LIBRARY_SCHEMA_PATH", str(BASE_DIR / "database" / "schema.sql"))
    DEFAULT_LOAN_DAYS = int(os.environ.get("DEFAULT_LOAN_DAYS", "14"))
    FINE_PER_DAY = float(os.environ.get("FINE_PER_DAY", "1.0"))
    TEMPLATES_PATH = BASE_DIR / "frontend" / "templates"
    STATIC_PATH = BASE_DIR / "frontend" / "assets"
