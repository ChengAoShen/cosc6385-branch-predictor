#!/usr/bin/env bash
# Bash replacement for the csh "run" script (needs no csh/dc).
# Usage: ./run.sh traces
set -e
cd "$(dirname "$0")"
[ -n "$1" ] || { echo "Usage: $0 <trace-file-directory>"; exit 1; }
make -C src predict >/dev/null
sum=0; n=0
for t in $(find "$1" -name '*.trace.*' | sort); do
  mpki=$(./src/predict "$t" | tail -1 | sed -e 's/ *MPKI//')
  printf "%-40s\t%0.3f\n" "$t" "$mpki"
  sum=$(awk -v a="$sum" -v b="$mpki" 'BEGIN{print a+b}'); n=$((n+1))
done
awk -v s="$sum" -v n="$n" 'BEGIN{printf "average MPKI: %.3f\n", s/n}'
