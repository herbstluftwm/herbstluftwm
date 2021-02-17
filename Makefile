## Wrapper around cmake

VERSION = $(shell cat VERSION)

.PHONY: all all-nodoc doc

BUILDDIR = build

all: $(BUILDDIR)
	cd $(BUILDDIR) && $(MAKE)
	@echo The compilation result can be found in $(BUILDDIR)/

$(BUILDDIR):
	mkdir -p $@
	cd $@ && cmake ..

clean:
	rm -r $(BUILDDIR)/
