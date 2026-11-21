#!/bin/sh
set -eu
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined -pthread \
  -Itools/cache_host_stubs -Imain/book \
  tools/cover_ram_cache_host_test.c main/book/cover_ram_cache.c -o "$out/test"
"$out/test"
