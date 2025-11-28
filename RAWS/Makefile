# RAWS Top-Level Makefile
#
# Usage:
#   make           # Build library (RAWS.a)
#   make test      # Build and run tests
#   make test-all  # Run full test suite
#   make all       # Build everything
#   make clean     # Clean all build artifacts

.PHONY: all lib test test-all clean help

help:
	@echo "RAWS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  lib       Build RAWS.a library"
	@echo "  test      Run sony decoder test"
	@echo "  test-all  Run full test suite (+ distortion)"
	@echo "  all       Build everything"
	@echo "  clean     Remove all build artifacts"

# Default: build library
lib:
	$(MAKE) -f Makefile.raws

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

# Clean everything
clean:
	$(MAKE) -f Makefile.raws clean
	$(MAKE) -f Makefile.sony clean
	rm -rf tmp/var
