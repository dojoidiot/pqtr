# PQTR Makefile
.PHONY: all pack test tidy labs base

PACK_DIR = tmp/pack

# Build and pack
all: labs base pack

# Build LABS (WebAssembly app)
labs:
	@echo "=== Building LABS ==="
	$(MAKE) -C LABS

# Build BASE (native server)
base:
	@echo "=== Building BASE ==="
	$(MAKE) -C BASE

# Pack deployment package
pack: labs base
	@echo "=== Packing ==="
	@mkdir -p $(PACK_DIR)/bin $(PACK_DIR)/etc $(PACK_DIR)/www $(PACK_DIR)/var/BASE $(PACK_DIR)/var/LABS
	@cp BASE/tmp/base $(PACK_DIR)/bin/
	@cp BASE/bin/base.sh $(PACK_DIR)/bin/
	@cp BASE/etc/*.json $(PACK_DIR)/etc/ 2>/dev/null || true
	@cp LABS/tmp/wasm/* $(PACK_DIR)/www/
	@echo "Packed to $(PACK_DIR)/"

# Run tests
test:
	@echo "=== LABS Unit Tests ==="
	$(MAKE) -C LABS test
	@echo "=== BASE Unit Tests ==="
	$(MAKE) -C BASE test

# Clean all
tidy:
	@echo "=== Tidying ==="
	$(MAKE) -C LABS tidy || true
	$(MAKE) -C BASE tidy || true
	rm -rf tmp
