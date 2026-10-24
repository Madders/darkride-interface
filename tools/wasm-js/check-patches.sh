#!/bin/sh
# Checks the changes to QuickJS-NG (patches/*.patch) natively, against the unpatched release.
#
#   tools/wasm-js/check-patches.sh [setup|build|test|compare|time|all]...    (all when none is given)
#
#   setup    unpacks the release twice under build/check/: pristine/ and patched/, the second with the patches
#            applied, as a git repository with one commit per patch (where a new patch is made), and configures a
#            native build of each (qjs, run-test262). A tree that exists is left as it is; to start again, delete
#            build/check/.
#   build    builds both natively (after an edit in patched/).
#   test     QuickJS-NG's own tests (tests.conf) on the patched build.
#   compare  runs every check/*.js with both builds' qjs, as a classic script and as a module: the outputs must
#            be the same.
#   time     times check/timing.cases on both sources (check/timing.c, the engine compiled in), three rounds in
#            turn, and prints the fastest of each with the ratio, unpatched to patched.
#
# A new patch: edit build/check/patched/quickjs-<version>/, then build, test, compare and time; commit it there,
# and write patches/<nnnn>-<name>.patch: what it changes and why, an empty line, then the output of
# `git -C build/check/patched/quickjs-<version> diff --text HEAD~1 HEAD`. build.sh applies it on its next run.
set -e
D=$(cd "$(dirname "$0")" && pwd)
# the version and archive as build.sh has them
QUICKJS_VERSION=$(sed -n 's/^QUICKJS_VERSION=//p' "$D/build.sh")
QUICKJS_SHA256=$(sed -n 's/^QUICKJS_SHA256=//p' "$D/build.sh")
ARCHIVE_NAME=quickjs-ng-v$QUICKJS_VERSION.tar.gz
C=$D/build/check
PRISTINE=$C/pristine/quickjs-$QUICKJS_VERSION
PATCHED=$C/patched/quickjs-$QUICKJS_VERSION

find_archive() {
    for DIR in "$D/../../../downloads/quickjs-ng-v$QUICKJS_VERSION" \
               "${TIVOLI_DOWNLOADS:-/nonexistent}/quickjs-ng-v$QUICKJS_VERSION" "$D/build/downloads"; do
        if [ -f "$DIR/$ARCHIVE_NAME" ]; then
            ARCHIVE="$DIR/$ARCHIVE_NAME"
            ACTUAL=$(shasum -a 256 "$ARCHIVE" | cut -d' ' -f1)
            [ "$ACTUAL" = "$QUICKJS_SHA256" ] || { echo "$ARCHIVE: SHA-256 $ACTUAL, expected $QUICKJS_SHA256" >&2; exit 1; }
            return
        fi
    done
    echo "no $ARCHIVE_NAME: run tools/wasm-js/build.sh once, which fetches it" >&2
    exit 1
}

setup() {
    find_archive
    if [ ! -f "$PRISTINE/quickjs.h" ]; then
        mkdir -p "$C/pristine"
        tar -xzmf "$ARCHIVE" -C "$C/pristine"
    fi
    if [ ! -f "$PATCHED/quickjs.h" ]; then
        mkdir -p "$C/patched"
        tar -xzmf "$ARCHIVE" -C "$C/patched"
        GIT="git -C $PATCHED -c user.name=check-patches -c user.email=check-patches@localhost"
        $GIT init -q
        $GIT add -A
        $GIT commit -q -m "QuickJS-NG v$QUICKJS_VERSION"
        for PATCH in "$D"/patches/*.patch; do
            [ -f "$PATCH" ] || continue
            patch -d "$PATCHED" -p1 -s < "$PATCH" || { echo "$PATCH does not apply" >&2; exit 1; }
            $GIT add -A
            $GIT commit -q -m "$(basename "$PATCH" .patch): $(head -1 "$PATCH")"
        done
    else
        echo "patched/ exists, left as it is ($(git -C "$PATCHED" log --oneline | wc -l | tr -d ' ') commits)"
    fi
    for TREE in pristine patched; do
        if [ ! -f "$C/native-$TREE/build.ninja" ]; then
            cmake -S "$C/$TREE/quickjs-$QUICKJS_VERSION" -B "$C/native-$TREE" -G Ninja -DCMAKE_BUILD_TYPE=Release \
                -DQJS_BUILD_EXAMPLES=OFF > "$C/native-$TREE.configure.log" 2>&1
        fi
    done
}

build() {
    for TREE in pristine patched; do
        nice -n 10 ninja -C "$C/native-$TREE" qjs run-test262 > "$C/native-$TREE.build.log" 2>&1 ||
            { tail -20 "$C/native-$TREE.build.log" >&2; echo "the $TREE build failed" >&2; exit 1; }
    done
    echo "built: $C/native-pristine, $C/native-patched"
}

run_tests() {
    if (cd "$PATCHED" && "$C/native-patched/run-test262" -c tests.conf > "$C/tests.log" 2>&1); then
        echo "QuickJS-NG's tests on the patched build: $(tail -1 "$C/tests.log")"
    else
        grep -v "^Result" "$C/tests.log" | tail -20 >&2
        echo "QuickJS-NG's tests FAILED on the patched build: $(tail -1 "$C/tests.log")" >&2
        exit 1
    fi
}

compare() {
    mkdir -p "$C/compare"
    DIFFERENT=0
    for SCRIPT in "$D"/check/*.js; do
        # as a classic script (sloppy mode, as the client runs scripts) and as a module (strict mode)
        for MODE in script module; do
            NAME=$(basename "$SCRIPT" .js).$MODE
            "$C/native-pristine/qjs" --$MODE "$SCRIPT" > "$C/compare/$NAME.pristine.txt" 2>&1 || true
            "$C/native-patched/qjs" --$MODE "$SCRIPT" > "$C/compare/$NAME.patched.txt" 2>&1 || true
            if cmp -s "$C/compare/$NAME.pristine.txt" "$C/compare/$NAME.patched.txt"; then
                echo "$(basename "$SCRIPT") as a $MODE: the same ($(wc -l < "$C/compare/$NAME.patched.txt" | tr -d ' ') lines)"
            else
                echo "$(basename "$SCRIPT") as a $MODE: DIFFERENT (< unpatched, > patched):"
                diff "$C/compare/$NAME.pristine.txt" "$C/compare/$NAME.patched.txt" | head -20
                DIFFERENT=1
            fi
        done
    done
    [ $DIFFERENT = 0 ] || exit 1
}

timing() {
    for TREE in pristine patched; do
        clang -O2 -w -D_GNU_SOURCE -I "$C/$TREE/quickjs-$QUICKJS_VERSION" -o "$C/timing-$TREE" "$D/check/timing.c" \
            "$C/$TREE/quickjs-$QUICKJS_VERSION/dtoa.c" "$C/$TREE/quickjs-$QUICKJS_VERSION/libregexp.c" \
            "$C/$TREE/quickjs-$QUICKJS_VERSION/libunicode.c" -lm &
    done
    wait
    : > "$C/timing.txt"
    for ROUND in 1 2 3; do
        for TREE in pristine patched; do
            "$C/timing-$TREE" "$D/check/timing.cases" | sed "s/^/$TREE /" >> "$C/timing.txt"
        done
    done
    echo "load: $(uptime | sed 's/.*load averages*: //')"
    awk '{ key = $2; if (!(key in order)) { order[key] = ++n; names[n] = key }
           t = $1 "/" key; if (!(t in best) || $3 < best[t]) best[t] = $3; result[t] = $4 }
         END { printf "%-26s %12s %12s %8s\n", "microseconds", "unpatched", "patched", "ratio"
               for (i = 1; i <= n; i++) { k = names[i]; p = best["pristine/" k]; q = best["patched/" k]
                   printf "%-26s %12.4f %12.4f %7.2fx%s\n", k, p, q, p / q,
                          (result["pristine/" k] == result["patched/" k]) ? "" : "  RESULTS DIFFER" } }' "$C/timing.txt"
}

[ $# -gt 0 ] || set -- all
for STEP in "$@"; do
    case "$STEP" in
        setup) setup ;;
        build) build ;;
        test) run_tests ;;
        compare) compare ;;
        time) timing ;;
        all) setup; build; run_tests; compare; timing ;;
        *) echo "usage: $0 [setup|build|test|compare|time|all]..." >&2; exit 2 ;;
    esac
done
