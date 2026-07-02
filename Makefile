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

$(BUILDDIR)/doc/hlwm-doc.json:
	make -C $(BUILDDIR)/doc doc_json

.PHONY: smoke-test
smoke-test: all $(BUILDDIR)/doc/hlwm-doc.json
	$(MAKE) tox EXTRA_TOX_ARGS="-m 'not exclude_from_coverage'"

.PHONY: long-test
long-test: all $(BUILDDIR)/doc/hlwm-doc.json
	$(MAKE) tox EXTRA_TOX_ARGS="-m 'exclude_from_coverage'"

.PHONY: test
test: smoke-test long-test

.PHONY: tox
tox: all
	cd $(BUILDDIR); tox -c ..  -- -v --maxfail=1 $(EXTRA_TOX_ARGS)

.PHONY: flake8
flake8:
	flake8

.PHONY: check-using-std
check-using-std:
	./ci/check-using-std.sh

.PHONY: check
check: check-using-std flake8 test
