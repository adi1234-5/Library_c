cmake_minimum_required(VERSION 3.20)
project(library_cpp_backend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(inja CONFIG QUIET)

add_executable(library_server
  backend/src/main.cpp
  backend/src/auth.cpp
  backend/src/database.cpp
  backend/src/pages/handlers_pages.cpp
  backend/src/api/handlers_api.cpp
  backend/src/render.cpp
  backend/src/session.cpp
  backend/src/utils.cpp
)

target_include_directories(library_server PRIVATE backend/include)

target_link_libraries(library_server PRIVATE
  SQLite::SQLite3
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
  Crow::Crow
)

if(TARGET pantor::inja)
  target_link_libraries(library_server PRIVATE pantor::inja)
  target_compile_definitions(library_server PRIVATE LIBRARY_HAS_INJA=1)
endif()
