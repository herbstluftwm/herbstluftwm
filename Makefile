## Wrapper around cmake

VERSION = $(shell cat VERSION)

TARFILE = herbstluftwm-$(VERSION).tar

.PHONY: all all-nodoc doc tar

BUILDDIR = build

all: $(BUILDDIR)
	cd $(BUILDDIR) && $(MAKE) -j$(nproc)
	@echo The compilation result can be found in $(BUILDDIR)/

$(BUILDDIR):
	mkdir -p $@
	cp valgrind-xephyr.sh "$@"
	cd $@ && cmake ..

clean:
	rm -r $(BUILDDIR)/

doc: $(BUILDDIR)
	cd $(BUILDDIR)/doc && $(MAKE)

tar: doc
	git archive --prefix=herbstluftwm-$(VERSION)/ -o $(TARFILE) HEAD
	tar --transform="flags=r;s,$(BUILDDIR),herbstluftwm-$(VERSION),"  --owner=0 --group=0 \
		-uvf $(TARFILE) $(BUILDDIR)/doc/*.html $(BUILDDIR)/doc/*.[1-9]
	gzip $(TARFILE)
	gpg --detach-sign $(TARFILE).gz
