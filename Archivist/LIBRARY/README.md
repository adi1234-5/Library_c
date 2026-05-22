# The Archivist - Dual Runtime Edition

This project keeps the new C++ backend architecture while restoring the original Flask/Python localhost runtime as a compatibility server. The website UI, SQLite schema, login flow, dashboards, CRUD workflows, form routes, and JSON APIs are shared across both runtimes.

## Project Structure

```text
LIBRARY/
  app.py
  server.py
  routes.py
  auth.py
  database.py
  config.py
  requirements.txt
  CMakeLists.txt
  frontend/
    assets/
    templates/
  database/
    schema.sql
    library.db
  backend/
    include/
    src/
      api/
      pages/
```

`frontend/templates/` is the website UI source. The C++ render layer reads those files and fills the dynamic sections for users, flash messages, books, circulation records, and dashboard data without changing the template design.

The Flask compatibility server also renders `frontend/templates/` directly, serves `frontend/assets/` at `/assets`, and uses `database/library.db`.

## Dependencies

### Python localhost runtime

Detect Python:

```powershell
python --version
```

Install Flask dependencies:

```powershell
python -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

Dependency list:

```text
Flask       Localhost web server, routing, template rendering, sessions
Werkzeug    Password hashing and Flask runtime utilities
SQLite      Python standard library sqlite3 module
```

### C++ backend runtime

Install the C++ dependencies with vcpkg:

```powershell
vcpkg install crow sqlite3 openssl nlohmann-json inja
```

C++ dependency list:

```text
Crow              HTTP server and routing
SQLite3           Database engine
OpenSSL Crypto    Password hashing and session token generation
nlohmann-json     JSON request/response handling
inja              Optional template dependency; current renderer has no runtime Python dependency
CMake 3.20+       Build configuration
C++20 compiler    Modern C++ backend build
```

## Run Flask Locally

Use this while the C++ server is incomplete or when you want the recovered original localhost workflow:

```powershell
python app.py
```

or:

```powershell
$env:FLASK_APP="app.py"
flask run
```

Open:

```text
http://localhost:5000
```

Default users:

```text
admin / admin123
student / student123
```

## Build C++

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

## Run C++

```powershell
.\build\Release\library_server.exe
```

Open:

```text
http://localhost:5000
```

## Database Setup

The server uses SQLite.

- If `database/library.db` already exists, it is reused unchanged.
- If `database/library.db` is missing or empty, the server applies `database/schema.sql`.
- Default users are seeded only when needed:
  - `admin / admin123`
  - `student / student123`

Existing Werkzeug `scrypt:32768:8:1$...` password hashes are supported by the C++ backend through OpenSSL.

## Environment Variables

```text
LIBRARY_DB_PATH       Optional path to SQLite DB. Defaults to ./database/library.db
SECRET_KEY           Reserved for deployment configuration
PORT                 Optional server port. Defaults to 5000
DEFAULT_LOAN_DAYS    Defaults to 14
FINE_PER_DAY          Defaults to 1.0
```

## Debugging Missing Localhost Dependencies

If `python app.py` fails because Python is missing, install Python 3.11+ and enable "Add python.exe to PATH".

If Flask is missing:

```powershell
python -m pip install -r requirements.txt
```

If templates are missing, confirm this folder exists:

```text
frontend/templates
```

If the database is missing or empty, the Flask server applies:

```text
database/schema.sql
```


## Preserved Routes

Page routes:

```text
/
/dashboard
/login
/signup
/logout
/admin/inventory
/admin/books/add
/admin/books/<id>/edit
/admin/books/<id>/delete
/admin/books/<id>/visibility
/admin/circulation
/admin/issue
/admin/return/<id>
/books/search
/student/dashboard
/student/issue/<id>
/student/return/<id>
```

API routes:

```text
/api/signup
/api/login
/api/logout
/api/books
/api/books/<id>
/api/issues
/api/issues/<id>/return
/api/me
```
