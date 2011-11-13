
include ../version.mk
include ../config.mk
include ../colors.mk

.PHONY: all clean install

TARGET = cip:wwwcip/herbstluftwm/
ASCIIDOCFLAGS = -a linkcss -a stylesdir=./ --theme main

all: index.html faq.html news.html herbstluftwm.html herbstclient.html

%.html: %.txt header.txt footer.txt
	$(call colorecho,DOC,$@)
	@$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

herbstluftwm.html: ../doc/herbstluftwm.txt
	$(call colorecho,DOC,$@)
	@$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

herbstclient.html: ../doc/herbstclient.txt
	$(call colorecho,DOC,$@)
	@$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

news.html: ../NEWS

clean:
	rm -f *.html

install:
	@echo scp index.html main.css $(TARGET)


