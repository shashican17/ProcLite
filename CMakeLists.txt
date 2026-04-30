cmake_minimum_required(VERSION 3.16)
project(ProcLite VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Compiler flags ────────────────────────────────────────────────────────────
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O2)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -fsanitize=address,undefined)
    add_link_options(-fsanitize=address,undefined)
endif()

# ── Source files ──────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/process.cpp
    src/parser.cpp
    src/system_stats.cpp
    src/process_manager.cpp
    src/ui.cpp
    src/logger.cpp
    src/utils.cpp
)

add_executable(proclite ${SOURCES})
target_include_directories(proclite PRIVATE include)

# ── Install target ────────────────────────────────────────────────────────────
install(TARGETS proclite DESTINATION bin)

# ── CPack for packaging ───────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME    "proclite")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_GENERATOR       "TGZ;DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "you@example.com")
include(CPack)
