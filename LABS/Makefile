# LABS Top-Level Makefile
#
# Usage:
#   make           # Build library (labs.a)
#   make test      # Run tests (mods)
#   make test-dawn # Run DAWN GPU shader tests
#   make all       # Build everything
#   make clean     # Clean all build artifacts

.PHONY: all lib test test-dawn clean help

help:
	@echo "LABS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  lib        Build labs.a library"
	@echo "  test       Run mods tests"
	@echo "  test-dawn  Run DAWN GPU shader tests (in VIBE)"
	@echo "  all        Build everything"
	@echo "  clean      Remove all build artifacts"
	@echo ""
	@echo "Note: TUNE (style optimizer) is now a separate project"

# Default: build library
lib:
	$(MAKE) -f Makefile.labs

# Tests
test: lib
	$(MAKE) -f Makefile.mods test

# DAWN tests (delegate to VIBE)
test-dawn:
	$(MAKE) -C ../VIBE test-dawn

# All: library + test binaries
all: lib
	$(MAKE) -f Makefile.mods all

# Clean everything
clean:
	$(MAKE) -f Makefile.labs clean
	$(MAKE) -f Makefile.mods clean
	rm -rf tmp/var
