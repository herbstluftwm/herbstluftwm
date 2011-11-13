
include ../version.mk
include ../config.mk
include ../colors.mk

.PHONY: all clean install

TARGET = cip:wwwcip/herbstluftwm/
ASCIIDOCFLAGS = -a linkcss -a stylesdir=./ --theme main

all: index.html herbstluftwm.html herbstclient.html

index.html: index.txt
	$(call colorecho,DOC,$@)
	@$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

herbstluftwm.html: ../doc/herbstluftwm.txt
	$(call colorecho,DOC,$@)
	@$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

herbstclient.html: ../doc/herbstclient.txt
	$(call colorecho,DOC,$@)
	@$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

clean:
	rm -f *.html

install:
	@echo scp index.html main.css $(TARGET)


