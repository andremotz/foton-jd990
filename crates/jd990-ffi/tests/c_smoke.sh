#!/bin/sh
# Baut die Static Library und den C-Smoke-Test und führt ihn aus.
set -e
cd "$(dirname "$0")/../../.."
cargo build -q -p jd990-ffi
# Rust-Staticlibs brauchen unter Linux die System-Bibliotheken ausdrücklich
case "$(uname)" in Linux) SYS="-lpthread -ldl -lm" ;; *) SYS="" ;; esac
cc -Wall -Wextra -Werror -std=c11 -I crates/jd990-ffi/include crates/jd990-ffi/tests/smoke.c \
   target/debug/libjd990_ffi.a $SYS -o target/debug/jd990-ffi-smoke
target/debug/jd990-ffi-smoke "$@"
