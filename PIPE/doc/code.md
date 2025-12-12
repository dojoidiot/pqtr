# Code Management

[back](../README.md)

## Working Directory

All output goes to `tmp/` (relative to PIPE root), never `/tmp`.

```bash
# Correct
./bin/tune img.ARW preview --save-area tmp/test

# Wrong
./bin/tune img.ARW preview --save-area /tmp/test
```

The `tmp/` directory structure:
```
tmp/
├── obj/<name>/    # Build objects (labs, mods, tune)
├── bin/<name>/    # Test binaries
└── var/<name>/    # Test output, tuning results
```

## Running Tune

Always set `LD_LIBRARY_PATH` for OpenCV:

```bash
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune img.ARW preview --save-area tmp/out
```

Common options:
| Option | Purpose |
|--------|---------|
| `--optimizer spsa` | Default, phased optimization |
| `--optimizer aceo` | Eigenspace search (needs covariance) |
| `--optimizer hybrid` | ACEO then SPSA polish |
| `--full` | All 45 dials in single pass |
| `--fine` | Save intermediate outputs |
| `--with-cov etc/aceo_full.json` | Use prior covariance |
| `--save-cov tmp/cov.json` | Save accumulated covariance |

## Building

```bash
make              # Build lib/pipe.a
make tune         # Build bin/tune, bin/labs + tests
make test         # Quick tests (mods + tune-fast)
make test-all     # Full test suite
make clean        # Clean all
```

## Test Images

Test images live in `var/pics/`. The embedded JPEG preview is the reference target.

```bash
# Single image test
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune var/pics/DSC00144.ARW preview --save-area tmp/test

# Batch test (all pics)
make test-batch
```

## Covariance Workflow

Build covariance with SPSA, refine with ACEO:

```bash
# Bootstrap: SPSA builds covariance
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune img1.ARW preview \
    --save-area tmp/cov --optimizer spsa --save-cov tmp/cov.json

# Refine: ACEO uses and updates covariance
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune img2.ARW preview \
    --save-area tmp/cov --optimizer aceo --with-cov tmp/cov.json --save-cov tmp/cov.json
```

The `bin/cvar.sh` script automates this workflow.

## Debugging

Save intermediate stages with `--fine`:

```bash
LD_LIBRARY_PATH=lib/opencv/build/lib ./bin/tune img.ARW preview \
    --save-area tmp/debug --fine --fine-area tmp/debug/stages
```

Check loss values in tune output - look for convergence pattern.

## See Also

- [tldr.md](./tldr.md) - Algorithm overview
- [tune.md](./tune.md) - Full tune API documentation
- [labs.md](./labs.md) - Library architecture
