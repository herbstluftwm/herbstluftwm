
.PHONY: all clean install

TARGET = herbstluftwm.org:www/

ASCIIDOC = asciidoc
ASCIIDOCFLAGS = -s
TXT = $(wildcard *.txt)
INSTALLFILES = $(TXT:.txt=.html) \
	herbstluftwm.html \
	herbstclient.html \
	herbstluftwm.svg \
	favicon.ico \
	main.css

LINKS = NEWS MIGRATION

all: $(INSTALLFILES) $(LINKS)

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

tutorial.html: ../doc/herbstluftwm-tutorial.txt

faq-toc.html: faq-content.html ./gentoc.sh
	./gentoc.sh $< > $@

faq.html: faq-toc.html

favicon.ico: ../share/herbstluftwm.svg
	convert -density 256x256 -background transparent $< \
        -define icon:auto-resize -colors 256 $@

# some symbolic links to make links in these asciidoc files work in www, too
$(LINKS):
	ln -sf $(shell echo $@ | tr A-Z a-z).html $@

clean:
	rm -f *.html *-content.html $(LINKS)

install: all
	rsync -v $(INSTALLFILES) $(TARGET)
