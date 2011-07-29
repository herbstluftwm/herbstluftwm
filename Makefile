
include config.mk

# project
SRCDIR = src
SRC = $(wildcard $(SRCDIR)/*.c)
HEADER = $(wildcard $(SRCDIR)/*.h)
OBJ = ${SRC:.c=.o}
TARGET = herbstluftwm
HERBSTCLIENTDOC = doc/herbstclient.txt

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

doc: doc/herbstclient.1 doc/herbstclient.html

doc/herbstclient.1: $(HERBSTCLIENTDOC)
	$(call colorecho,DOC,doc/herbstclient.1)
	@a2x -f manpage $<

doc/herbstclient.html: $(HERBSTCLIENTDOC)
	$(call colorecho,DOC,doc/herbstclient.html)
	@asciidoc $<

