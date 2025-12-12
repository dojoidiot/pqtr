# PIPE Top-Level Makefile
#
# Usage:
#   make           # Build library (pipe.a)
#   make test      # Run tests (mods)
#   make test-wgpu # Run WGPU GPU shader tests
#   make all       # Build everything
#   make clean     # Clean all build artifacts

.PHONY: all lib test test-wgpu clean help

help:
	@echo "PIPE Build System"
	@echo ""
	@echo "Targets:"
	@echo "  lib        Build pipe.a library"
	@echo "  test       Run mods tests"
	@echo "  test-wgpu  Run WGPU GPU shader tests (in VIBE)"
	@echo "  all        Build everything"
	@echo "  clean      Remove all build artifacts"
	@echo ""
	@echo "Note: TUNE (style optimizer) is now a separate project"

# Default: build library
lib:
	$(MAKE) -f Makefile.pipe

# Tests
test: lib
	$(MAKE) -f Makefile.mods test

# WGPU tests (delegate to VIBE)
test-wgpu:
	$(MAKE) -C ../VIBE test-dawn

# All: library + test binaries
all: lib
	$(MAKE) -f Makefile.mods all

# Clean everything
clean:
	$(MAKE) -f Makefile.pipe clean
	$(MAKE) -f Makefile.mods clean
	rm -rf tmp/var
