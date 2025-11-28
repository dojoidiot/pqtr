# RAWS Top-Level Makefile
#
# Usage:
#   make           # Build library (raws.a)
#   make test      # Build and run tests
#   make all       # Build everything
#   make clean     # Clean all build artifacts

.PHONY: all lib test clean help

help:
	@echo "RAWS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  lib     Build raws.a library"
	@echo "  test    Build and run sony test"
	@echo "  all     Build everything"
	@echo "  clean   Remove all build artifacts"

# Default: build library
lib:
	$(MAKE) -f Makefile.raws

# Test: build library, then test binaries, then run
test: lib
	$(MAKE) -f Makefile.sony
	@echo ""
	@echo "Running sony decoder test..."
	LD_LIBRARY_PATH=../LABS/lib/opencv/build/lib tmp/bin/sony var/sony.ARW

# All: library + tests
all: lib
	$(MAKE) -f Makefile.sony

# Clean everything
clean:
	$(MAKE) -f Makefile.raws clean
	$(MAKE) -f Makefile.sony clean
	rm -rf tmp/var
