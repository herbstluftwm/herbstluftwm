
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

.PHONY: doc cleandoc

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

