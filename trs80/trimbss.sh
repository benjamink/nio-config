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
# Every stored section (code, rodata, data) must end at or before bss: the file stops at bss, and crt0 clears
# whatever lies beyond it, so code placed there (the linker puts sections crt0.asm does not list after bss) is
# zeroed. Calling such a routine runs a NOP slide. Check each non-empty section's tail, with no exceptions.
for sec in $(awk '/^__(code|rodata|data)_[a-z0-9_]+_head / {sub(/^__/,"",$1); sub(/_head$/,"",$1); print $1}' "$map"); do
  h=$(awk -v n="__${sec}_head" '$1==n {gsub(/[$]/,"",$3); print $3}' "$map")
  t=$(awk -v n="__${sec}_tail" '$1==n {gsub(/[$]/,"",$3); print $3}' "$map")
  [ "$h" != "$t" ] || continue
  [ $((0x$t)) -le "$bss" ] || { echo "$1: section $sec ($h-$t) lies after bss at $(printf %X $bss): add SECTION $sec to crt0.asm before the bss sections" >&2; exit 1; }
done
head -c $((bss - 0x3000)) "$bin" > "$bin.tmp" && mv "$bin.tmp" "$bin"
