#!/usr/bin/env python3
"""Analyze dial settings across batch tune results to find mean style."""

import json
import os
import numpy as np
from pathlib import Path

def load_tune_json(path):
    """Load tune.json and extract dial values."""
    with open(path) as f:
        data = json.load(f)

    # Extract dial values from first link's modules
    dials = {}
    if 'links' in data and len(data['links']) > 0:
        modules = data['links'][0].get('modules', {})

        # Flatten nested module structure
        for mod_name, mod_data in modules.items():
            if isinstance(mod_data, dict):
                for key, value in mod_data.items():
                    if isinstance(value, (int, float)):
                        dials[f"{mod_name}.{key}"] = value
                    elif isinstance(value, dict):
                        # selective_color has nested hue/sat/lum
                        for subkey, subval in value.items():
                            if isinstance(subval, (int, float)):
                                dials[f"{mod_name}.{key}.{subkey}"] = subval

    return dials

def main():
    batch_dir = Path('tmp/var/batch')

    # Find all tune.json files
    tune_files = list(batch_dir.glob('*/tune.json'))
    print(f"Found {len(tune_files)} tune results\n")

    # Collect all dial values
    all_dials = {}

    for tune_file in sorted(tune_files):
        name = tune_file.parent.name
        dials = load_tune_json(tune_file)

        print(f"{name}: {len(dials)} dials")

        for dial_name, value in dials.items():
            if dial_name not in all_dials:
                all_dials[dial_name] = []
            all_dials[dial_name].append(value)

    print(f"\n{'='*60}")
    print("DIAL STATISTICS (mean ± std)")
    print(f"{'='*60}")
    print(f"{'Dial':<25} {'Mean':>8} {'Std':>8} {'Min':>8} {'Max':>8}  Deviation")
    print("-" * 70)

    # Sort by deviation from 0.5 (neutral)
    dial_stats = []
    for dial_name, values in sorted(all_dials.items()):
        arr = np.array(values)
        mean = arr.mean()
        std = arr.std()
        deviation = abs(mean - 0.5)
        dial_stats.append((dial_name, mean, std, arr.min(), arr.max(), deviation))

    # Sort by deviation (largest first)
    dial_stats.sort(key=lambda x: -x[5])

    for dial_name, mean, std, min_v, max_v, deviation in dial_stats:
        bar = '*' * int(deviation * 40)
        print(f"{dial_name:<25} {mean:>8.4f} {std:>8.4f} {min_v:>8.4f} {max_v:>8.4f}  {bar}")

    print(f"\n{'='*60}")
    print("TOP DEVIATIONS FROM NEUTRAL (0.5)")
    print(f"{'='*60}")

    significant = [(d, m, s) for d, m, s, _, _, dev in dial_stats if dev > 0.02]
    for dial_name, mean, std in significant[:10]:
        direction = "HIGH" if mean > 0.5 else "LOW"
        print(f"  {dial_name}: {mean:.4f} ({direction} by {abs(mean-0.5):.4f}, std={std:.4f})")

    # Output mean settings as JSON
    mean_settings = {dial: float(np.mean(values)) for dial, values in all_dials.items()}

    print(f"\n{'='*60}")
    print("MEAN STYLE PRESET (etc/style_standard.json)")
    print(f"{'='*60}")
    print(json.dumps({"name": "Sony A7III Standard", "dials": mean_settings}, indent=2))

if __name__ == '__main__':
    main()
