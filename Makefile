## Wrapper around cmake

VERSION = $(shell cat VERSION)

TARFILE = herbstluftwm-$(VERSION).tar

.PHONY: all all-nodoc doc tar

BUILDDIR = build

all: $(BUILDDIR)
	cd $(BUILDDIR) && $(MAKE)
	@echo The compilation result can be found in $(BUILDDIR)/

$(BUILDDIR):
	mkdir -p $@
	cd $@ && cmake ..

clean:
	rm -r $(BUILDDIR)/

doc: $(BUILDDIR)
	cd $(BUILDDIR)/doc && $(MAKE)

tar: doc
	git archive --prefix=herbstluftwm-$(VERSION)/ -o $(TARFILE) HEAD
	tar --transform="flags=r;s,$(BUILDDIR),herbstluftwm-$(VERSION),"  --owner=0 --group=0 \
		-uvf $(TARFILE) $(BUILDDIR)/doc/*.{html,json} $(BUILDDIR)/doc/*.[1-9]
	gzip $(TARFILE)
	gpg --detach-sign $(TARFILE).gz
