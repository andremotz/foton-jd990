#!/bin/sh
# Baut die Static Library und den C-Smoke-Test und führt ihn aus.
# Unter Windows (Git Bash/MSYS) wird MSVC benutzt: cl statt cc, Bibliothek
# jd990_ffi.lib statt libjd990_ffi.a. Voraussetzung ist eine MSVC-Umgebung
# (cl/link und INCLUDE/LIB) — in der CI liefert das msvc-dev-cmd, lokal die
# Entwicklereingabeaufforderung von Visual Studio.
set -e
cd "$(dirname "$0")/../../.."
case "$(uname -s)" in
MINGW*|MSYS*)
    # Rust mit statischer C-Runtime bauen (+crt-static), damit die Objekte zur
    # Staticlib zu cl passen (Default /MT). System-Bibliotheken wie von
    # `cargo rustc -- --print native-static-libs` (x86_64-pc-windows-msvc).
    # -wd4127: do-while(0)-Makro (CHECK) — reine Makro-Idiomatik, kein Fehler.
    RUSTFLAGS="-C target-feature=+crt-static" cargo build -q -p jd990-ffi
    cl -nologo -W4 -WX -wd4127 -D_CRT_SECURE_NO_WARNINGS -std:c11 \
       -I crates/jd990-ffi/include crates/jd990-ffi/tests/smoke.c \
       target/debug/jd990_ffi.lib \
       ws2_32.lib userenv.lib bcrypt.lib ntdll.lib advapi32.lib kernel32.lib dbghelp.lib \
       -Fo:target/debug/ -Fe:target/debug/jd990-ffi-smoke.exe
    EXE=".exe"
    ;;
*)
    cargo build -q -p jd990-ffi
    # Rust-Staticlibs brauchen unter Linux die System-Bibliotheken ausdrücklich
    case "$(uname)" in Linux) SYS="-lpthread -ldl -lm" ;; *) SYS="" ;; esac
    cc -Wall -Wextra -Werror -std=c11 -I crates/jd990-ffi/include crates/jd990-ffi/tests/smoke.c \
       target/debug/libjd990_ffi.a $SYS -o target/debug/jd990-ffi-smoke
    EXE=""
    ;;
esac
target/debug/jd990-ffi-smoke$EXE "$@"
