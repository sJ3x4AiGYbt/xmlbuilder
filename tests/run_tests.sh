#!/usr/bin/env bash
set -uo pipefail

cd "$(dirname "$0")/.."

TMP_DIR="tests/tmp"
mkdir -p "$TMP_DIR"

FAILURES=0

run_xmlbuilder() {
    local name="$1" xml="$2"
    local db="$TMP_DIR/${name}.db" sql="$TMP_DIR/${name}.sql" log="$TMP_DIR/${name}.log"
    rm -f "$db" "$sql"
    if ./xmlbuilder "$xml" "$db" "$sql" > "$log" 2>&1; then
        echo "OK   (run): $name"
    else
        echo "FAIL (run): $name"
        cat "$log"
        FAILURES=$((FAILURES + 1))
        return 1
    fi
}

assert_eq() {
    local desc="$1" expected="$2" actual="$3"
    if [ "$expected" = "$actual" ]; then
        echo "OK   : $desc"
    else
        echo "FAIL : $desc (expected '$expected', got '$actual')"
        FAILURES=$((FAILURES + 1))
    fi
}

sql_rows() {
    # runs a query and joins the result rows with '|'
    local db="$1" query="$2"
    sqlite3 "$db" "$query" | paste -sd '|' -
}

echo "==> make"
make -s || { echo "FAIL : build"; exit 1; }

echo
echo "==> example.xml (base case, no repetition under the same parent)"
run_xmlbuilder example tests/fixtures/example.xml
assert_eq "example: book count" "2" "$(sqlite3 "$TMP_DIR/example.db" 'SELECT COUNT(*) FROM book;')"
assert_eq "example: review_note column folded into book" "18|16" "$(sql_rows "$TMP_DIR/example.db" 'SELECT review_note FROM book ORDER BY id_book;')"

echo
echo "==> repeated_children.xml (multiple <review> under the same <book>)"
run_xmlbuilder repeated tests/fixtures/repeated_children.xml
assert_eq "repeated: book count" "1" "$(sqlite3 "$TMP_DIR/repeated.db" 'SELECT COUNT(*) FROM book;')"
assert_eq "repeated: review count (no data loss)" "2" "$(sqlite3 "$TMP_DIR/repeated.db" 'SELECT COUNT(*) FROM review;')"
assert_eq "repeated: review values" "12|15" "$(sql_rows "$TMP_DIR/repeated.db" 'SELECT note FROM review ORDER BY note;')"
assert_eq "repeated: book_review join table populated" "2" "$(sqlite3 "$TMP_DIR/repeated.db" 'SELECT COUNT(*) FROM book_review;')"

echo
if [ "$FAILURES" -eq 0 ]; then
    echo "All tests passed."
    exit 0
else
    echo "$FAILURES test(s) failed."
    exit 1
fi
