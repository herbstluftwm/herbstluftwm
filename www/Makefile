
include ../version.mk
include ../config.mk
include ../colors.mk

.PHONY: all clean install

TARGET = cip:wwwcip/herbstluftwm/
ASCIIDOCFLAGS = -s
TXT = $(wildcard *.txt)

all: $(TXT:.txt=.html) herbstluftwm.html herbstclient.html herbstluftwm.svg

%-content.html: %.txt
	$(call colorecho,DOC,$@)
	$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

%.html: %-content.html compose.sh
	./compose.sh $< > $@

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
	@echo scp index.html main.css $(TARGET)
