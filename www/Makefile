
include ../version.mk
include ../config.mk
include ../colors.mk

.PHONY: all clean install

TARGET = herbstluftwm.org:www/

ASCIIDOCFLAGS = -s
TXT = $(wildcard *.txt)
INSTALLFILES = $(TXT:.txt=.html) \
	herbstluftwm.html \
	herbstclient.html \
	herbstluftwm.svg

all: $(INSTALLFILES)

%-content.html: %.txt
	$(call colorecho,DOC,$@)
	$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

%.html: %-content.html compose.py
	./compose.py $< > $@

herbstluftwm.svg: ../share/herbstluftwm.svg
	cp $^ $@

herbstluftwm-content.html: ../doc/herbstluftwm.txt
	$(call colorecho,DOC,$@)
	@$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

herbstclient-content.html: ../doc/herbstclient.txt
	$(call colorecho,DOC,$@)
	@$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

news.html: ../NEWS

clean:
	rm -f *.html *-content.html

install:
	scp $(INSTALLFILES) $(TARGET)
