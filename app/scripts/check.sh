#!/usr/bin/env bash
# Solar 42N gatekeeper: configure + build + unit tests + pluginval (+ render
# regression once goldens exist, M8+). Run from anywhere.
set -euo pipefail

APP_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$APP_DIR/build"
CONFIG="${CONFIG:-RelWithDebInfo}"
STRICTNESS="${PLUGINVAL_STRICTNESS:-10}"  # release gate (M9c P1)

echo "== configure =="
cmake -S "$APP_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "== build =="
cmake --build "$BUILD_DIR" --parallel

echo "== unit tests =="
ctest --test-dir "$BUILD_DIR" --output-on-failure

echo "== pluginval =="
PLUGINVAL="${PLUGINVAL:-}"
if [ -z "$PLUGINVAL" ]; then
    for cand in \
        "$APP_DIR/tools/pluginval/bin/pluginval.app/Contents/MacOS/pluginval" \
        "/Applications/pluginval.app/Contents/MacOS/pluginval" \
        "$(command -v pluginval || true)"; do
        if [ -n "$cand" ] && [ -x "$cand" ]; then PLUGINVAL="$cand"; break; fi
    done
fi

VST3="$BUILD_DIR/Solar42N_artefacts/$CONFIG/VST3/Solar42N.vst3"
if [ -n "$PLUGINVAL" ] && [ -d "$VST3" ]; then
    "$PLUGINVAL" --strictness-level "$STRICTNESS" --skip-gui-tests --validate "$VST3"
else
    echo "WARNING: pluginval not found (or VST3 missing) — skipped."
    echo "  Install: download pluginval_macOS.zip from github.com/Tracktion/pluginval"
    echo "  and unzip into $APP_DIR/tools/pluginval/bin/"
fi

echo "== auval (AU) =="
AU_SRC="$BUILD_DIR/Solar42N_artefacts/$CONFIG/AU/Solar42N.component"
AU_DST="$HOME/Library/Audio/Plug-Ins/Components/Solar42N.component"
if command -v auval > /dev/null && [ -d "$AU_SRC" ]; then
    mkdir -p "$(dirname "$AU_DST")"
    rsync -a --delete "$AU_SRC/" "$AU_DST/"
    # A freshly copied component can hit a stale AudioComponentRegistrar
    # cache ("didn't find the component") — reset it and retry once.
    if ! auval -v aumu S42n S42p; then
        killall -9 AudioComponentRegistrar 2> /dev/null || true
        auval -v aumu S42n S42p
    fi
else
    echo "WARNING: auval or AU component missing — skipped."
fi

echo "== render regression =="
if [ -x "$BUILD_DIR/solar42n_render" ]; then
    "$BUILD_DIR/solar42n_render" "$BUILD_DIR/render-smoke.wav" 2
else
    echo "solar42n_render not built — skipped."
fi

echo "== check.sh: ALL GREEN =="
