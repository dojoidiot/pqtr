# LABS Top-Level Makefile
#
# Usage:
#   make           # Build library (labs.a)
#   make tune      # Build bin/tune + tests
#   make test      # Run quick tests (mods + tune-fast)
#   make test-all  # Run full test suite
#   make all       # Build everything
#   make clean     # Clean all build artifacts

.PHONY: all lib tune test test-all clean help

help:
	@echo "LABS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  lib       Build labs.a library"
	@echo "  tune      Build bin/tune + tests (optimizer)"
	@echo "  test      Run quick tests (mods + tune-fast)"
	@echo "  test-all  Run full test suite (+ tune-full + tune-rnd)"
	@echo "  all       Build everything"
	@echo "  clean     Remove all build artifacts"

# Default: build library
lib:
	$(MAKE) -f Makefile.labs

# Build bin/tune and tests
tune: lib
	$(MAKE) -f Makefile.tune

# Quick tests
test: lib
	$(MAKE) -f Makefile.mods test
	$(MAKE) -f Makefile.tune test-fast

# Full test suite
test-all: lib
	$(MAKE) -f Makefile.mods test
	$(MAKE) -f Makefile.tune test-full

# All: library + test binaries
all: lib
	$(MAKE) -f Makefile.mods all
	$(MAKE) -f Makefile.tune all

# Clean everything
clean:
	$(MAKE) -f Makefile.labs clean
	$(MAKE) -f Makefile.mods clean
	$(MAKE) -f Makefile.tune clean
	rm -rf tmp/var
