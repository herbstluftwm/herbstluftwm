
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
	herbstluftwm.svg \
	main.css

all: $(INSTALLFILES)

%-content.html: %.txt
	$(call colorecho,DOC,$@)
	$(ASCIIDOC) $(ASCIIDOCFLAGS) -o $@ $<

%.html: %-content.html compose.py
	./compose.py $< > $@

herbstluftwm.svg: ../share/herbstluftwm.svg
	cp $^ $@

news.html: ../NEWS

migration.html: ../MIGRATION

herbstluftwm.html: ../doc/herbstluftwm.txt

herbstclient.html: ../doc/herbstclient.txt

tour.html: ../doc/herbstluftwm-tutorial.txt

clean:
	rm -f *.html *-content.html

install: all
	rsync -v $(INSTALLFILES) $(TARGET)
