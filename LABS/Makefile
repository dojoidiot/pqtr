# LABS Top-Level Makefile
#
# Usage:
#   make           # Build library (labs.a)
#   make test      # Build and run quick tests
#   make test-all  # Build and run full test suite
#   make all       # Build everything
#   make clean     # Clean all build artifacts

.PHONY: all lib test test-all clean help

help:
	@echo "LABS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  lib       Build labs.a library"
	@echo "  test      Run quick tests (mods + pipe)"
	@echo "  test-all  Run full test suite (+ diff + tune)"
	@echo "  all       Build everything"
	@echo "  clean     Remove all build artifacts"

# Default: build library
lib:
	$(MAKE) -f Makefile.labs

# Quick tests
test: lib
	$(MAKE) -f Makefile.test test

# Full test suite
test-all: lib
	$(MAKE) -f Makefile.test test-all

# All: library + test binaries
all: lib
	$(MAKE) -f Makefile.test all

# Clean everything
clean:
	$(MAKE) -f Makefile.labs clean
	$(MAKE) -f Makefile.test clean
	rm -rf tmp/var
