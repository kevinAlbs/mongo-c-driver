cmake_minimum_required (VERSION 3.15...4.0)

if (NOT ENABLE_RUST)
    return ()
endif ()

# Link libmongoc_rust.a with --whole-archive so that all exported Rust symbols
# are retained in the shared library even though no C code calls them directly.
macro (mongoc_rust_link_library TARGET LIBRARY)
    if (UNIX AND NOT APPLE)
        target_link_libraries (${TARGET} INTERFACE bson::shared
            "-Wl,--whole-archive" "${LIBRARY}" "-Wl,--no-whole-archive")
    elseif (APPLE)
        target_link_libraries (${TARGET} INTERFACE bson::shared
            "-Wl,-force_load,${LIBRARY}")
    else ()
        target_link_libraries (${TARGET} INTERFACE bson::shared "${LIBRARY}")
    endif ()
endmacro ()

if (ENABLE_RUST_SYSTEM)
    if (NOT DEFINED MONGOC_RUST_LIBRARY)
        message (FATAL_ERROR
            "ENABLE_RUST_SYSTEM requires -DMONGOC_RUST_LIBRARY=<path to libmongoc_rust.a>")
    endif ()
    if (NOT DEFINED MONGOC_RUST_INCLUDE_DIR)
        message (FATAL_ERROR
            "ENABLE_RUST_SYSTEM requires -DMONGOC_RUST_INCLUDE_DIR=<dir containing mongoc/mongoc-rust-private.h>")
    endif ()
    if (NOT EXISTS "${MONGOC_RUST_LIBRARY}")
        message (FATAL_ERROR "MONGOC_RUST_LIBRARY not found: ${MONGOC_RUST_LIBRARY}")
    endif ()
    if (NOT EXISTS "${MONGOC_RUST_INCLUDE_DIR}/mongoc/mongoc-rust-private.h")
        message (FATAL_ERROR
            "mongoc-rust-private.h not found at ${MONGOC_RUST_INCLUDE_DIR}/mongoc/mongoc-rust-private.h")
    endif ()

    add_library (mongoc-rust INTERFACE)
    mongoc_rust_link_library (mongoc-rust "${MONGOC_RUST_LIBRARY}")
    target_include_directories (mongoc-rust INTERFACE "${MONGOC_RUST_INCLUDE_DIR}")

    message (STATUS "Using pre-built mongoc-rust library: ${MONGOC_RUST_LIBRARY}")
    return ()
endif ()

find_program (CARGO_EXECUTABLE cargo REQUIRED)
find_program (CARGO_CBINDGEN_EXECUTABLE cbindgen REQUIRED)

set (MONGOC_RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/target")
set (MONGOC_RUST_LIBRARY "${MONGOC_RUST_TARGET_DIR}/debug/libmongoc_rust.a")
set (MONGOC_RUST_HEADER "${CMAKE_CURRENT_BINARY_DIR}/include/mongoc/mongoc-rust-private.h")

file (
    GLOB_RECURSE MONGOC_RUST_SOURCES
    LIST_DIRECTORIES false
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    CONFIGURE_DEPENDS
    "src/*.rs"
)

add_custom_command (
    OUTPUT "${MONGOC_RUST_LIBRARY}"
    COMMAND "${CARGO_EXECUTABLE}" build --target-dir "${MONGOC_RUST_TARGET_DIR}" --lib
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS
        "Cargo.toml"
        "${MONGOC_RUST_SOURCES}"
)

add_custom_command (
    OUTPUT "${MONGOC_RUST_HEADER}"
    COMMAND "${CARGO_CBINDGEN_EXECUTABLE}" --config cbindgen.toml --output "${MONGOC_RUST_HEADER}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    DEPENDS
        "cbindgen.toml"
        "${MONGOC_RUST_LIBRARY}"
)

add_custom_target (mongoc-rust-target DEPENDS "${MONGOC_RUST_LIBRARY}" "${MONGOC_RUST_HEADER}")

add_library (mongoc-rust INTERFACE)
add_dependencies(mongoc-rust mongoc-rust-target)
mongoc_rust_link_library (mongoc-rust "${MONGOC_RUST_LIBRARY}")
target_include_directories(mongoc-rust INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/include")

message (STATUS "Building mongoc-rust... done.")
