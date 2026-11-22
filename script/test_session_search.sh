#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
TEST_DIR="$(mktemp -d)"
trap 'rm -rf "$TEST_DIR"' EXIT
xcrun clang++ -std=c++17 -fobjc-arc -Wl,-dead_strip -framework Cocoa \
  -lcurl -lsqlite3 -IWechatExporter -IWechatExporter/core \
  tests/session_search_tests.mm WechatExporter/SessionDataSource.mm \
  WechatExporter/core/Utils.cpp -o "$TEST_DIR/session_search_tests"
"$TEST_DIR/session_search_tests"
