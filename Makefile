# PQTR Makefile
#
# Usage:
#   make        Build all projects
#   make test   Build and run test server (calls test.sh)
#   make tidy   Clean all build artifacts

.PHONY: all test tidy labs base

all: labs base

labs:
	$(MAKE) -C LABS -f Makefile.wasm

base:
	$(MAKE) -C BASE

test:
	@./test.sh

tidy:
	$(MAKE) -C LABS -f Makefile.wasm clean 2>/dev/null || true
	$(MAKE) -C BASE clean 2>/dev/null || true
	rm -rf tmp/test
	rm -f BASE/www/labs.html BASE/www/labs.js BASE/www/labs.wasm
