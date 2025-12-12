# PQTR Master Makefile
#
# Builds all projects in dependency order.
# Run from repository root.
#
# Usage:
#   make           # Build everything (wire + raws + labs + tune + desk)
#   make raws      # Build GEAR library
#   make labs      # Build PIPE library + tune/labs binaries
#   make tune      # Build PIPE tune binary only
#   make desk      # Build DESK app (builds PIPE first)
#   make test      # Run all tests
#   make clean     # Clean all projects
#   make rewire    # Remove and recreate all symlinks

.PHONY: all help wire rewire raws labs tune desk test test-raws test-labs clean

# Default: build everything
all: desk

help:
	@echo "PQTR Build System"
	@echo ""
	@echo "Build targets:"
	@echo "  all       Build everything (default)"
	@echo "  raws      Build GEAR library"
	@echo "  labs      Build PIPE library + tune/labs binaries"
	@echo "  tune      Build PIPE tune binary only"
	@echo "  desk      Build DESK application"
	@echo ""
	@echo "Run targets:"
	@echo "  ./desk.sh       Launch DESK GUI"
	@echo "  ./tune.sh ...   Run tune optimizer"
	@echo "  ./labs.sh ...   Run labs processor"
	@echo ""
	@echo "Test targets:"
	@echo "  test      Run all tests (GEAR + PIPE)"
	@echo "  test-raws Run GEAR decoder test"
	@echo "  test-labs Run PIPE test suite"
	@echo ""
	@echo "Other:"
	@echo "  wire      Create symlinks between projects"
	@echo "  rewire    Remove and recreate symlinks"
	@echo "  clean     Clean all build artifacts"

# Wiring (creates symlinks between projects)
wire:
	@bash ./wire.sh

rewire:
	@bash ./wire.sh --unwire
	@bash ./wire.sh

# ============================================================
# Build targets
# ============================================================

# GEAR: decoder library (no dependencies)
raws: wire
	@echo ""
	@echo "=== Building GEAR ==="
	$(MAKE) -C GEAR lib

# PIPE: core library + binaries (depends on GEAR)
labs: raws
	@echo ""
	@echo "=== Building PIPE ==="
	$(MAKE) -C PIPE lib
	$(MAKE) -C PIPE -f Makefile.tune tune labs

# TUNE: just the tune binary
tune: labs
	@echo ""
	@echo "=== Building TUNE ==="
	$(MAKE) -C PIPE -f Makefile.tune tune

# DESK: GUI application (depends on PIPE)
desk: labs
	@echo ""
	@echo "=== Building DESK ==="
	$(MAKE) -C DESK -f Makefile.desk

# ============================================================
# Test targets
# ============================================================

# Run all tests
test: test-raws test-labs

# GEAR test
test-raws: raws
	@echo ""
	@echo "=== Testing GEAR ==="
	$(MAKE) -C GEAR test

# PIPE tests
test-labs: labs
	@echo ""
	@echo "=== Testing PIPE ==="
	$(MAKE) -C PIPE test-all

# ============================================================
# Clean
# ============================================================

clean:
	$(MAKE) -C GEAR clean 2>/dev/null || true
	$(MAKE) -C PIPE clean 2>/dev/null || true
	$(MAKE) -C DESK -f Makefile.desk clean 2>/dev/null || true
