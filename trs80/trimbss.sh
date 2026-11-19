#!/bin/sh
# trimbss.sh <name>: cut build/<name>.bin at the start of bss (crt0 clears it at start-up, it is not stored).
# Fails if any stored section lies after bss or if bss would reach the client at D000h.
set -e
bin="build/$1.bin"; map="build/$1.map"
val() { awk -v n="$1" '$1==n {gsub(/[$]/,"",$3); print $3}' "$map"; }
bss=$((0x$(val __bss_compiler_head)))
tail=$((0x$(val __tail)))
# The stack grows down from D000h into the top of bss with no guard. The deepest call chain (hosts save through
# the app-store client) needs about 221 bytes, and LS-DOS SVCs and interrupts share the stack: keep a reserve.
reserve=${STACK_RESERVE:-384}
[ "$tail" -le $((0xD000 - reserve)) ] || { echo "$1: image ends at $(printf %X $tail): less than $reserve bytes of stack below D000h" >&2; exit 1; }
[ "$tail" -le $((0xD000)) ] || { echo "$1: image ends at $(printf %X $tail), past the client at D000h" >&2; exit 1; }
for t in $(awk '/^__(code|rodata|data)_[a-z0-9_]+_tail / && $1 != "__code_l_tail" {gsub(/[$]/,"",$3); print $3}' "$map"); do
  [ $((0x$t)) -le "$bss" ] || { echo "$1: a stored section ends at $t, after bss at $(printf %X $bss): add it to crt0.asm" >&2; exit 1; }
done
head -c $((bss - 0x3000)) "$bin" > "$bin.tmp" && mv "$bin.tmp" "$bin"
