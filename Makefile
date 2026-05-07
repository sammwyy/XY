EXAMPLES := render_images audio input_status debug_text custom_font tasks render_3d voxel_game

.PHONY: all build clean clean-all clean-one run $(EXAMPLES)

all: $(EXAMPLES)

ifneq (,$(filter run build clean,$(MAKECMDGOALS)))
# When invoking `make run <example_id>`, treat the extra goals as arguments,
# so we don't also build them as standalone targets.
$(EXAMPLES):
	@:
else
$(EXAMPLES):
	$(MAKE) -C examples/$@ all
endif

EXAMPLE_ARG := $(word 2,$(MAKECMDGOALS))

build:
	@if [ -z "$(EXAMPLE_ARG)" ] || [ "$(EXAMPLE_ARG)" = "all" ]; then \
		echo "Usage: make build <example_id>"; \
		echo "Examples: $(EXAMPLES)"; \
		exit 2; \
	fi
	@if [ ! -d "./examples/$(EXAMPLE_ARG)" ]; then \
		echo "Unknown example_id: $(EXAMPLE_ARG)"; \
		echo "Known examples: $(EXAMPLES)"; \
		exit 2; \
	fi
	$(MAKE) -C "examples/$(EXAMPLE_ARG)" all

# Usage:
#   make run <example_id>
# Example:
#   make run custom_font
run: build
	@if [ -z "$(EXAMPLE_ARG)" ] || [ "$(EXAMPLE_ARG)" = "all" ]; then \
		echo "Usage: make run <example_id>"; \
		echo "Examples: $(EXAMPLES)"; \
		exit 2; \
	fi
	@if [ ! -d "./examples/$(EXAMPLE_ARG)" ]; then \
		echo "Unknown example_id: $(EXAMPLE_ARG)"; \
		echo "Known examples: $(EXAMPLES)"; \
		exit 2; \
	fi
	@if ! command -v flatpak >/dev/null 2>&1; then \
		echo "flatpak not found in PATH (required to run PCSX2)"; \
		exit 127; \
	fi
	flatpak run --filesystem="$(CURDIR):rw" net.pcsx2.PCSX2 \
		"$(CURDIR)/examples/$(EXAMPLE_ARG)/$(EXAMPLE_ARG).elf"

clean:
	@:

ifeq ($(strip $(EXAMPLE_ARG)),)
clean: clean-all
else ifeq ($(EXAMPLE_ARG),all)
clean: clean-all
else
clean: clean-one
endif

clean-all:
	for example in $(EXAMPLES); do $(MAKE) -C examples/$$example clean; done

clean-one:
	@if [ ! -d "./examples/$(EXAMPLE_ARG)" ]; then \
		echo "Unknown example_id: $(EXAMPLE_ARG)"; \
		echo "Known examples: $(EXAMPLES)"; \
		exit 2; \
	fi
	$(MAKE) -C "examples/$(EXAMPLE_ARG)" clean

# Swallow the extra goal passed to `make run <example_id>`.
%:
	@:
