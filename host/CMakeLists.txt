# ─────────────────────────────────────────────────────────────────────────────
#  FotonJD990 — JUCE-Host (M3: generisches UI, zunächst nur Standalone)
#
#  cmake -S host -B host/build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
#  cmake --build host/build -j
#  Windows (Visual Studio): cmake -S host -B host/build && cmake --build host/build --config Release
#
#  JUCE liegt unter host/JUCE (gitignored). Version: 9.0.2 (immer das neueste Release;
#  Klonbefehl in README.md).
#  Der Rust-Kern (crates/jd990-ffi) wird als Static Library mitgebaut.
# ─────────────────────────────────────────────────────────────────────────────
cmake_minimum_required(VERSION 3.22)
project(FotonJD990 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "")

# Windows/MSVC: statische C-Runtime (/MT bzw. /MTd) — passend zur Rust-Staticlib,
# die mit `-C target-feature=+crt-static` gebaut wird (design.md Entscheidung 4:
# keine VC-Redist nötig). Muss vor JUCE stehen, damit auch dessen Ziele es erben
# (JUCE wählt danach seine vorgebauten Bibliotheken: MT/MTd/MD/MDd).
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(JUCE)

# ── Rust-Kern ────────────────────────────────────────────────────────────────
find_program(CARGO cargo HINTS /opt/homebrew/opt/rustup/bin $ENV{HOME}/.cargo/bin $ENV{USERPROFILE}/.cargo/bin REQUIRED)
get_filename_component(CARGO_DIR ${CARGO} DIRECTORY)   # rustc liegt daneben (Homebrew-rustup ist keg-only)
set(RUST_TARGET_DIR ${CMAKE_BINARY_DIR}/rust)
if(MSVC)
    # MSVC: die Staticlib heißt jd990_ffi.lib (kein lib-Präfix, .lib statt .a)
    # und wird mit statischer C-Runtime gebaut — konsistent zu /MT oben.
    set(RUST_LIB ${RUST_TARGET_DIR}/release/jd990_ffi.lib)
    set(RUST_ENV ${CMAKE_COMMAND} -E env
        "CARGO_TARGET_DIR=${RUST_TARGET_DIR}"
        "RUSTFLAGS=-C target-feature=+crt-static")
else()
    set(RUST_LIB ${RUST_TARGET_DIR}/release/libjd990_ffi.a)
    set(RUST_ENV ${CMAKE_COMMAND} -E env
        "PATH=${CARGO_DIR}:$ENV{PATH}"
        "CARGO_TARGET_DIR=${RUST_TARGET_DIR}")
endif()
add_custom_target(jd990_ffi_build
    COMMAND ${RUST_ENV} ${CARGO} build --release -p jd990-ffi
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/..
    BYPRODUCTS ${RUST_LIB}
    COMMENT "Rust: jd990-ffi")
add_library(jd990_ffi STATIC IMPORTED)
set_target_properties(jd990_ffi PROPERTIES
    IMPORTED_LOCATION ${RUST_LIB}
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/../crates/jd990-ffi/include)
add_dependencies(jd990_ffi jd990_ffi_build)
if(MSVC)
    # Rust-Staticlib unter MSVC: System-Bibliotheken ausdrücklich dazulinken.
    # Stand von `cargo rustc --target x86_64-pc-windows-msvc -- --print
    # native-static-libs` (kernel32/ntdll/userenv/ws2_32/dbghelp), um bcrypt
    # und advapi32 ergänzt (Hash-Zufall/Registry, je nach std-Fläche) —
    # zu viel dazugelinkte Bibliotheken sind harmlos, fehlende sind fatal.
    set_target_properties(jd990_ffi PROPERTIES INTERFACE_LINK_LIBRARIES
        "ws2_32;userenv;bcrypt;ntdll;advapi32;kernel32;dbghelp")
elseif(UNIX AND NOT APPLE)
    # Rust-Staticlib unter Linux: System-Bibliotheken ausdrücklich dazulinken
    set_target_properties(jd990_ffi PROPERTIES INTERFACE_LINK_LIBRARIES "pthread;dl;m")
endif()

# ── App ──────────────────────────────────────────────────────────────────────
juce_add_gui_app(FotonJD990
    PRODUCT_NAME "FotonJD990"
    COMPANY_NAME "andremotz"
    BUNDLE_ID "art.prometheus.fotonjd990"
    ICON_BIG "${CMAKE_SOURCE_DIR}/../design/icon/icon-1024.png")   # erzeugt mit design/icon/make_icon.swift

target_sources(FotonJD990 PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/Model.cpp
    Source/Controls.cpp
    Source/Panels.cpp
    Source/Envelope.cpp
    Source/PerformanceView.cpp
    Source/LibraryView.cpp
    Source/DeviceLink.cpp)

target_compile_definitions(FotonJD990 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:FotonJD990,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:FotonJD990,JUCE_VERSION>")

add_dependencies(FotonJD990 jd990_ffi_build)
target_link_libraries(FotonJD990
    PRIVATE
        jd990_ffi
        juce::juce_gui_basics
        juce::juce_audio_devices
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    # Quellen sind UTF-8 (deutsche Umlaute, „…”): ohne /utf-8 rät MSVC nach
    # Systemcodepage und warnt (C4566/C4828) oder verfälscht Zeichen.
    target_compile_options(FotonJD990 PRIVATE /utf-8)
endif()
