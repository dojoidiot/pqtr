# Diff - Loss Metrics

[back](../README.md)

> **Note:** Diff functionality is now part of the unified [geos](./geos.md) module.
> The `geos::Task` class provides both loss measurement (`diff()`) and optimization (`run()`).

## Quick Reference

```cpp
#include <geos.hpp>

// Create task with target image
pqtr::Hold<geos::Task> task = geos::make(target);

// Measure loss
geos::Data metrics = task->diff(candidate);

// Visual difference
geos::View diffImg = task->view(candidate, 5.0f);
```

## Metrics

| Metric | Range | Meaning |
|--------|-------|---------|
| `spectral` | [0, ∞) | Weighted L2 distance in 19D feature space (0 = identical style) |
| `frequency` | [0, ∞) | Relative Laplacian variance difference (0 = identical sharpness) |

## Loss Function

**Weighted L2 loss**:
```
Loss = Σ weights[i] × (feature[i] - target[i])²
```

Feature weights are loaded from `etc/cnst.json`. Critical features (std_L, percentiles, color cast) have high weights.

## See Also

- [geos.md](./geos.md) - Full GeoS documentation (19D feature space, weighted L2 loss)
- [tldr.md](./tldr.md) - Quick overview
- [edge.md](./edge.md) - Frequency loss theory
