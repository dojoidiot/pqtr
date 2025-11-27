# Diff - Loss Metrics

[back](../README.md)

> **Note:** Diff functionality is now part of the unified [tune](./tune.md) module.
> The `tune::Task` class provides both loss measurement (`diff()`) and optimization (`run()`).

## Quick Reference

```cpp
#include <tune.hpp>

// Create task with target image
pqtr::Hold<tune::Task> task = tune::make(target);

// Measure loss (was diff::Task::diff)
tune::Data metrics = task->diff(candidate);

// Visual difference (was diff::Task::view)
tune::View diffImg = task->view(candidate, 5.0f);
```

## Metrics

| Metric | Range | Meaning |
|--------|-------|---------|
| `spectral` | [0, 1] | Geodesic distance on style hypersphere (0 = identical color/tone) |
| `frequency` | [0, ∞) | Relative Laplacian variance difference (0 = identical sharpness) |

## See Also

- [tune.md](./tune.md) - Full API documentation
- [geos.md](./geos.md) - Spectral loss theory
- [edge.md](./edge.md) - Frequency loss theory
