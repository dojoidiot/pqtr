#!/bin/bash
# cov.sh - Test dial covariance across sample images
# Usage: ./opt/cov.sh
# Output: tmp/opt/cov_matrix.json, tmp/opt/cov_report.txt

set -e
cd "$(dirname "$0")/.."

OUT="tmp/opt"
PICS="var/pics"

mkdir -p "$OUT/tunes"

echo "=== Step 1: Tune all images (skip-lut for pure dial optimization) ==="

for raw in "$PICS"/*.ARW; do
    name=$(basename "$raw" .ARW)
    dest="$OUT/tunes/$name"

    if [ -f "$dest/tune.json" ]; then
        echo "  $name: already tuned, skipping"
        continue
    fi

    echo "  $name: tuning..."
    mkdir -p "$dest"
    LD_LIBRARY_PATH=lib/opencv/build/lib bin/tune "$raw" preview \
        --save-area "$dest" --skip-lut --threshold 0.01 2>&1 | tail -5
done

echo ""
echo "=== Step 2: Extract dials and compute covariance ==="

python3 - "$OUT" <<'PYTHON'
import json, glob, sys, os
import numpy as np

out_dir = sys.argv[1]

# Dial names for reporting
DIAL_NAMES = [
    # color_correction (0-2)
    'exposure', 'temperature', 'tint',
    # tone_mapping (3-9)
    'contrast', 'highlights', 'shadows', 'toe_pivot', 'shoulder_pivot', 'white_point', 'black_point',
    # global_color (10-12)
    'vibrance', 'saturation', 'density',
    # split_tone (13-16)
    'shadow_temp', 'shadow_tint', 'highlight_temp', 'highlight_tint',
    # selective_color (17-40): 8 hues x 3
    'red_hue', 'red_sat', 'red_lum',
    'orange_hue', 'orange_sat', 'orange_lum',
    'yellow_hue', 'yellow_sat', 'yellow_lum',
    'green_hue', 'green_sat', 'green_lum',
    'cyan_hue', 'cyan_sat', 'cyan_lum',
    'blue_hue', 'blue_sat', 'blue_lum',
    'purple_hue', 'purple_sat', 'purple_lum',
    'magenta_hue', 'magenta_sat', 'magenta_lum',
]

def extract_dials(tune_json):
    """Extract 41 dials as flat vector from tune.json"""
    with open(tune_json) as f:
        data = json.load(f)

    # Use display link (index 1)
    link = data['links'][1]
    m = link['modules']

    dials = []
    # color_correction: 3
    dials += [m['color_correction'][k] for k in ['exposure','temperature','tint']]
    # tone_mapping: 7
    dials += [m['tone_mapping'][k] for k in ['contrast','highlights','shadows',
              'toe_pivot','shoulder_pivot','white_point','black_point']]
    # global_color: 3
    dials += [m['global_color'][k] for k in ['vibrance','saturation','density']]
    # split_tone: 4
    dials += [m['split_tone'][k] for k in ['shadow_temp','shadow_tint',
              'highlight_temp','highlight_tint']]
    # selective_color: 8 hues x 3 = 24
    for hue in ['red','orange','yellow','green','cyan','blue','purple','magenta']:
        dials += [m['selective_color'][hue][k] for k in ['hue','sat','lum']]

    return np.array(dials)

# Load all tune.json files
files = sorted(glob.glob(f'{out_dir}/tunes/*/tune.json'))
if len(files) < 3:
    print(f"ERROR: Need at least 3 images, found {len(files)}")
    sys.exit(1)

names = [os.path.basename(os.path.dirname(f)) for f in files]
print(f"Loaded {len(files)} tune results: {', '.join(names)}")

matrix = np.array([extract_dials(f) for f in files])  # N x 41
n_images, n_dials = matrix.shape
print(f"Matrix shape: {n_images} images x {n_dials} dials")

# Compute correlation matrix
corr = np.corrcoef(matrix.T)  # 41 x 41

# Save full matrix as JSON
cov_data = {
    'images': names,
    'dials': DIAL_NAMES,
    'dial_means': matrix.mean(axis=0).tolist(),
    'dial_stds': matrix.std(axis=0).tolist(),
    'correlation_matrix': corr.tolist(),
}
with open(f'{out_dir}/cov_matrix.json', 'w') as f:
    json.dump(cov_data, f, indent=2)

# Generate report
report = []
report.append("=" * 60)
report.append("DIAL COVARIANCE ANALYSIS")
report.append("=" * 60)
report.append(f"\nImages analyzed: {n_images}")
report.append(f"Dials per image: {n_dials}")
report.append("")

# Find strong correlations (|r| > 0.3)
np.fill_diagonal(corr, 0)
strong_threshold = 0.3
pairs = []
for i in range(n_dials):
    for j in range(i+1, n_dials):
        r = corr[i, j]
        if abs(r) > strong_threshold:
            pairs.append((i, j, r))

pairs.sort(key=lambda x: -abs(x[2]))

report.append(f"Strong correlations (|r| > {strong_threshold}): {len(pairs)} pairs")
report.append("-" * 60)

if pairs:
    for i, j, r in pairs:
        sign = "+" if r > 0 else "-"
        report.append(f"  {DIAL_NAMES[i]:20} <-> {DIAL_NAMES[j]:20}  r={r:+.3f}")
else:
    report.append("  (none found)")

report.append("")

# Summary statistics
max_corr = np.abs(corr).max()
mean_abs_corr = np.abs(corr[np.triu_indices(n_dials, k=1)]).mean()

report.append("-" * 60)
report.append("Summary:")
report.append(f"  Max |correlation|:  {max_corr:.3f}")
report.append(f"  Mean |correlation|: {mean_abs_corr:.3f}")
report.append("")

# Decision
if len(pairs) >= 5 or max_corr > 0.5:
    report.append("VERDICT: Strong covariance detected. CMA-ES likely beneficial.")
elif len(pairs) >= 2:
    report.append("VERDICT: Moderate covariance. CMA-ES may help marginally.")
else:
    report.append("VERDICT: Weak covariance. SPSA is already near-optimal.")

report.append("=" * 60)

report_text = "\n".join(report)
print(report_text)

with open(f'{out_dir}/cov_report.txt', 'w') as f:
    f.write(report_text)

print(f"\nSaved: {out_dir}/cov_matrix.json")
print(f"Saved: {out_dir}/cov_report.txt")
PYTHON
