#include "app_config.hpp"
#include "database.hpp"
#include "handlers.hpp"
#include "session.hpp"

#include <crow.h>

#include <iostream>

int main() {
  try {
    auto config = library::AppConfig::from_environment();
    library::Database db(config);
    db.ensure_initialized();

    crow::SimpleApp app;
    library::SessionStore sessions;
    library::register_page_routes(app, db, sessions);
    library::register_api_routes(app, db, sessions);

    std::cout << "The Archivist C++ backend running at http://localhost:" << config.port << "\n";
    app.port(static_cast<uint16_t>(config.port)).multithreaded().run();
  } catch (const std::exception& ex) {
    std::cerr << "Startup failed: " << ex.what() << "\n";
    return 1;
  }
  return 0;
}
