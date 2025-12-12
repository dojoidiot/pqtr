# GEAR Top-Level Makefile
#
# Usage:
#   make           # Build library (GEAR.a)
#   make test      # Build and run tests
#   make test-all  # Run full test suite
#   make all       # Build everything
#   make clean     # Clean all build artifacts

.PHONY: all lib test test-all test-dawn clean help

help:
	@echo "GEAR Build System"
	@echo ""
	@echo "Targets:"
	@echo "  lib        Build GEAR.a library"
	@echo "  test       Run sony decoder test"
	@echo "  test-all   Run full test suite (+ distortion)"
	@echo "  test-dawn  Run DAWN GPU shader tests"
	@echo "  all        Build everything"
	@echo "  clean      Remove all build artifacts"

# Default: build library
lib:
	$(MAKE) -f Makefile.gear

# Quick test
test: lib
	$(MAKE) -f Makefile.sony test

# Full test suite
test-all: lib
	$(MAKE) -f Makefile.sony test
	$(MAKE) -f Makefile.sony test-dist

# All: library + test binaries
all: lib
	$(MAKE) -f Makefile.sony all

# DAWN shader tests
test-dawn:
	$(MAKE) -f Makefile.dawn test

# Clean everything
clean:
	$(MAKE) -f Makefile.gear clean
	$(MAKE) -f Makefile.sony clean
	$(MAKE) -f Makefile.dawn clean 2>/dev/null || true
	rm -rf tmp/var
