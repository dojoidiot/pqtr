# PQTR Master Makefile
#
# Builds all projects in dependency order.
# Run from repository root.
#
# Usage:
#   make           # Build everything (wire + raws + labs + desk)
#   make raws      # Build RAWS library
#   make labs      # Build LABS library (builds RAWS first)
#   make desk      # Build DESK app (builds LABS first)
#   make test      # Run all tests
#   make clean     # Clean all projects
#   make rewire    # Remove and recreate all symlinks

.PHONY: all help wire rewire raws labs desk test test-raws test-labs clean

help:
	@echo "PQTR Build System"
	@echo ""
	@echo "Build targets:"
	@echo "  all       Build everything (default)"
	@echo "  raws      Build RAWS library"
	@echo "  labs      Build LABS library"
	@echo "  desk      Build DESK application"
	@echo ""
	@echo "Test targets:"
	@echo "  test      Run all tests (RAWS + LABS)"
	@echo "  test-raws Run RAWS decoder test"
	@echo "  test-labs Run LABS test suite"
	@echo ""
	@echo "Other:"
	@echo "  wire      Create symlinks between projects"
	@echo "  rewire    Remove and recreate symlinks"
	@echo "  clean     Clean all build artifacts"

# Default: build everything
all: desk

# Wiring (creates symlinks between projects)
wire:
	@bash ./wire.sh

rewire:
	@bash ./wire.sh --unwire
	@bash ./wire.sh

# ============================================================
# Build targets
# ============================================================

# RAWS: decoder library (no dependencies)
raws: wire
	@echo ""
	@echo "=== Building RAWS ==="
	$(MAKE) -C RAWS lib

# LABS: core library (depends on RAWS)
labs: raws
	@echo ""
	@echo "=== Building LABS ==="
	$(MAKE) -C LABS lib

# DESK: GUI application (depends on LABS)
desk: labs
	@echo ""
	@echo "=== Building DESK ==="
	$(MAKE) -C DESK -f Makefile.desk

# ============================================================
# Test targets
# ============================================================

# Run all tests
test: test-raws test-labs

# RAWS test
test-raws: raws
	@echo ""
	@echo "=== Testing RAWS ==="
	$(MAKE) -C RAWS test

# LABS tests
test-labs: labs
	@echo ""
	@echo "=== Testing LABS ==="
	$(MAKE) -C LABS test-all

# ============================================================
# Clean
# ============================================================

clean:
	$(MAKE) -C RAWS clean 2>/dev/null || true
	$(MAKE) -C LABS clean 2>/dev/null || true
	$(MAKE) -C DESK -f Makefile.desk clean 2>/dev/null || true
