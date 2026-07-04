#!/usr/bin/env bash
# Regenerate the .hex assembler goldens from the .spn fixtures using asfv1
# (the reference assembler: pip install asfv1, or a downloaded asfv1.py via
# ASFV1="python3 /path/to/asfv1.py"). Run from this directory.
set -euo pipefail
ASFV1=${ASFV1:-asfv1}

for spn in *.spn; do
    base=${spn%.spn}
    tmp=$(mktemp)
    $ASFV1 -q "$spn" "$tmp"
    python3 - "$tmp" "$base.hex" <<'EOF'
import struct, sys
words = struct.unpack('>128I', open(sys.argv[1], 'rb').read())
open(sys.argv[2], 'w').write('\n'.join('%08X' % w for w in words) + '\n')
EOF
    rm -f "$tmp"
    echo "$base.hex"
done
