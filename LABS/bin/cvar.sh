#!/bin/bash
# cvar.sh - Build covariance model from a directory of RAW images
#
# Usage: ./bin/cvar.sh <image_dir>
#        ./bin/cvar.sh var/pics
#
# Workflow:
#   1. SPSA bootstrap: First 2 images with SPSA (explores full 45D space)
#   2. ACEO refinement: Remaining images with ACEO (uses SPSA prior)
#
# Outputs etc/aceo_full.json as the standard 45-dial covariance model.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LABS_DIR="$(dirname "$SCRIPT_DIR")"
cd "$LABS_DIR"

# Check args
if [ -z "$1" ]; then
    echo "Usage: $0 <image_dir>"
    echo "  Builds etc/aceo_full.json from RAW images in <image_dir>"
    exit 1
fi

IMAGE_DIR="$1"
if [ ! -d "$IMAGE_DIR" ]; then
    echo "Error: Directory not found: $IMAGE_DIR"
    exit 1
fi

# Output paths
TMP_COV="tmp/cvar_accumulated.json"
FINAL_COV="etc/aceo_full.json"
mkdir -p tmp

# Find RAW images
IMAGES=$(find "$IMAGE_DIR" -maxdepth 1 -type f \( -iname "*.ARW" -o -iname "*.CR2" -o -iname "*.NEF" -o -iname "*.DNG" -o -iname "*.RAF" \) | sort)
COUNT=$(echo "$IMAGES" | grep -c . || echo 0)

if [ "$COUNT" -eq 0 ]; then
    echo "Error: No RAW images found in $IMAGE_DIR"
    exit 1
fi

# Determine bootstrap count (min 2 for SPSA, but not more than total)
BOOT_COUNT=2
if [ "$COUNT" -lt 2 ]; then
    BOOT_COUNT="$COUNT"
fi

echo "=== CVAR: Building covariance model ==="
echo "Images: $COUNT in $IMAGE_DIR"
echo "Bootstrap: $BOOT_COUNT images with SPSA (full 45D exploration)"
echo "Refinement: $((COUNT - BOOT_COUNT)) images with ACEO"
echo "Output: $FINAL_COV"
echo ""

# Set library path
export LD_LIBRARY_PATH="$LABS_DIR/lib/opencv/build/lib:$LD_LIBRARY_PATH"

# Process each image
N=0
for IMG in $IMAGES; do
    N=$((N + 1))
    NAME=$(basename "$IMG")

    if [ "$N" -le "$BOOT_COUNT" ]; then
        # SPSA bootstrap phase (explores full 45D dial space)
        echo "[$N/$COUNT] $NAME (SPSA bootstrap)"

        if [ "$N" -eq 1 ]; then
            # First image: no prior
            ./bin/tune "$IMG" preview --save-area /tmp \
                --optimizer spsa \
                --save-cov "$TMP_COV" \
                2>&1 | grep -E "^\[(SPSA|GEOS|BEST)" || true
        else
            # Second image: SPSA writes fresh covariance each run
            # Save to a temp file and we'll merge later
            ./bin/tune "$IMG" preview --save-area /tmp \
                --optimizer spsa \
                --save-cov "${TMP_COV}.2" \
                2>&1 | grep -E "^\[(SPSA|GEOS|BEST)" || true

            # Simple merge: use second file (it has independent samples)
            # TODO: proper merge would combine both matrices
            if [ -f "${TMP_COV}.2" ]; then
                mv "${TMP_COV}.2" "$TMP_COV"
            fi
        fi
    else
        # ACEO refinement phase (uses SPSA-built prior)
        echo "[$N/$COUNT] $NAME (ACEO with prior)"

        ./bin/tune "$IMG" preview --save-area /tmp \
            --optimizer aceo \
            --with-cov "$TMP_COV" \
            --save-cov "$TMP_COV" \
            2>&1 | grep -E "^\[ACEO" || true
    fi
done

# Move final result to etc/
if [ -f "$TMP_COV" ]; then
    mv "$TMP_COV" "$FINAL_COV"
    echo ""
    echo "=== Done ==="
    echo "Covariance model saved to: $FINAL_COV"
    echo "Bootstrap samples from $BOOT_COUNT SPSA runs"
    echo "Refinement from $((COUNT - BOOT_COUNT)) ACEO runs"
else
    echo "Error: No covariance file generated"
    exit 1
fi
