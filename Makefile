
include config.mk

# project
SRCDIR = src
SRC = $(wildcard $(SRCDIR)/*.c)
HEADER = $(wildcard $(SRCDIR)/*.h)
OBJ = ${SRC:.c=.o}
TARGET = herbstluftwm
HERBSTCLIENTDOC = doc/herbstclient.txt
HERBSTLUFTWMDOC = doc/herbstluftwm.txt

include rules.mk

all: build-herbstclient doc
clean: clean-herbstclient cleandoc

.PHONY: doc cleandoc install

cleandoc:
	$(call colorecho,RM,doc/herbstclient.1)
	@rm -f doc/herbstclient.1
	$(call colorecho,RM,doc/herbstclient.html)
	@rm -f doc/herbstclient.html

build-herbstclient:
	make -C ipc-client

clean-herbstclient:
	make -C ipc-client clean

doc: doc/herbstclient.1 doc/herbstclient.html doc/herbstluftwm.1 doc/herbstluftwm.html

doc/%.1: doc/%.txt
	$(call colorecho,DOC,$@)
	@a2x -f manpage $<

doc/%.html: doc/%.txt
	$(call colorecho,DOC,$@)
	@asciidoc $<

install: all
	@echo creating dirs...
	mkdir -p $(PREFIX)
	mkdir -p $(LICENSEDIR)
	mkdir -p $(BINDIR)
	mkdir -p $(MANDIR)
	mkdir -p $(DOCDIR)
	mkdir -p $(ETCDIR)
	mkdir -p $(ETCDIR)/bash_completion.d/
	mkdir -p $(CONFIGDIR)
	@echo copyiing files...
	install $(TARGET) $(BINDIR)
	install ipc-client/herbstclient $(BINDIR)/
	install -m 644 LICENSE $(LICENSEDIR)
	install -m 644 doc/herbstclient.1 $(MANDIR)/
	install -m 644 doc/herbstluftwm.1 $(MANDIR)/
	install -m 644 doc/herbstclient.html $(DOCDIR)/
	install -m 644 doc/herbstluftwm.html $(DOCDIR)/
	install -m 644 BUGS $(DOCDIR)/
	install -m 644 share/herbstluftrc $(CONFIGDIR)/
	install -m 644 share/herbstclient-completion $(ETCDIR)/bash_completion.d/

