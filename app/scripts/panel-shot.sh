#!/usr/bin/env bash
# Panel eye-check screenshot (M9b tile loop): launches the standalone fresh,
# captures its window to <out.png>, quits it.
#
#   app/scripts/panel-shot.sh /tmp/panel.png
#
# Tile-diff workflow used for M9b P1/P1.5 (see 00-LOG 2026-07-07): the window
# content below the chrome (~104 px at the default 902x635 window, i.e. rows
# after title bar + mute banner + preset bar) is the 4950x3200 panel fit to
# width. Crop matching tiles from the shot and from
# reference-docs/solar42n-panel-1.png (whole image = panel) at the SAME
# fractional coordinates and compare side by side:
#   shot:  x = fx*W,  y = chrome + fy*(W/1.547)
#   ref:   x = fx*1949, y = fy*1260
# sips crops with:  sips img --cropOffset Y X --cropToHeightWidth H W --out t.png
# (offsets must be >= 1 — a 0 offset silently falls back to a centre crop).
set -uo pipefail
OUT="${1:?usage: panel-shot.sh <out.png>}"
APP="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/build/Solar42N_artefacts/RelWithDebInfo/Standalone/Solar42N.app"

# A fresh instance must not race a terminating one: `open` during teardown
# gets swallowed and no window ever appears.
pkill -x Solar42N 2>/dev/null
for _ in $(seq 1 10); do pgrep -x Solar42N >/dev/null || break; sleep 1; done
sleep 1
open "$APP"

WID=""
for _ in $(seq 1 15); do
  sleep 3
  WID=$(swift - <<'EOF'
import CoreGraphics
let opts = CGWindowListOption([.optionOnScreenOnly, .excludeDesktopElements])
if let list = CGWindowListCopyWindowInfo(opts, kCGNullWindowID) as? [[String: Any]] {
    for w in list {
        if let owner = w[kCGWindowOwnerName as String] as? String, owner == "Solar42N",
           let num = w[kCGWindowNumber as String] as? Int,
           let b = w[kCGWindowBounds as String] as? [String: Any],
           let h = b["Height"] as? Double, h > 200 { print(num); break }
    }
}
EOF
) || WID=""
  [ -n "$WID" ] && break
done
if [ -z "$WID" ]; then
  echo "panel-shot: window not found (launch race?) — run again" >&2
  pkill -x Solar42N 2>/dev/null || true
  exit 1
fi
sleep 1
screencapture -x -o -l "$WID" "$OUT"
pkill -x Solar42N 2>/dev/null || true
echo "panel-shot: captured $OUT (window $WID)"
