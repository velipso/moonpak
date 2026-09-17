# SPDX-License-Identifier: 0BSD

ROOT_DIR := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

# find all examples/* directories that have a Makefile in them
EXAMPLES := $(notdir $(patsubst %/,%,$(dir $(wildcard $(ROOT_DIR)/examples/*/Makefile))))

.PHONY: all clean $(EXAMPLES)

all: $(EXAMPLES)

clean:
	find "$(ROOT_DIR)/examples" -mindepth 2 -maxdepth 2 -type d -name build -exec rm -rf -- {} +

$(EXAMPLES):
	@$(MAKE) -C $(ROOT_DIR)/examples/$@
