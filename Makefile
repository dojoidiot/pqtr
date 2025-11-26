# PQTR Master Makefile
#
# Builds all MAINs in dependency order.
# Run from repository root.
#
# Usage:
#   make          # Build everything (wire + labs + desk)
#   make labs     # Build LABS only
#   make desk     # Build DESK (builds LABS first)
#   make raws     # Build RAWS standalone test
#   make clean    # Clean all projects
#   make rewire   # Remove and recreate all symlinks

.PHONY: all wire rewire raws labs desk raws-test clean clean-wire

# Default: build everything
all: desk

# Wiring (creates symlinks between projects)
wire:
	@bash ./wire.sh

rewire:
	@bash ./wire.sh --unwire
	@bash ./wire.sh

# RAWS: decoder library (no dependencies)
raws: wire
	@echo "=== Building RAWS ==="
	$(MAKE) -C RAWS -f Makefile.raws

# LABS: core library (depends on RAWS library)
labs: raws
	@echo "=== Building LABS ==="
	$(MAKE) -C LABS -f Makefile.labs

# DESK: GUI application (depends on LABS library)
desk: labs
	@echo "=== Building DESK ==="
	$(MAKE) -C DESK -f Makefile.desk

# RAWS test binary (optional, for decoder development)
raws-test: raws
	@echo "=== Building RAWS test ==="
	$(MAKE) -C RAWS -f Makefile.sony

# Clean all projects
clean:
	$(MAKE) -C RAWS -f Makefile.raws clean
	$(MAKE) -C RAWS -f Makefile.sony clean
	$(MAKE) -C LABS -f Makefile.labs clean
	$(MAKE) -C DESK -f Makefile.desk clean

clean-wire:
	@bash ./wire.sh --unwire
