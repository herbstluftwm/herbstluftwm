
# paths
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -Isrc/ -I/usr/include -I${X11INC}  `pkg-config --cflags glib-2.0`
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11 `pkg-config --libs glib-2.0`

ifeq ($(shell uname),Linux)
LIBS += -lrt
endif

# FLAGS
LD = gcc
CC = gcc
CFLAGS = -g -std=c99 -pedantic -Wall ${INCS} -D _XOPEN_SOURCE=600
VERSIONFLAGS = \
    -D HERBSTLUFT_VERSION=$(VERSION) \
    -D HERBSTLUFT_VERSION_MAJOR=$(VERSION_MAJOR) \
    -D HERBSTLUFT_VERSION_MINOR=$(VERSION_MINOR)
CFLAGS += $(VERSIONFLAGS)
LDFLAGS = -g
INSTALLDIR = /
PREFIX = $(INSTALLDIR)/usr/
BINDIR = $(PREFIX)/bin
SHAREDIR = $(PREFIX)/share
MANDIR = $(SHAREDIR)/man/man1
DOCDIR = $(SHAREDIR)/doc/herbstluftwm
EXAMPLESDIR = $(DOCDIR)/examples
LICENSEDIR = $(SHAREDIR)/licenses/herbstluftwm
ETCDIR = $(INSTALLDIR)/etc/
CONFIGDIR = $(ETCDIR)/xdg/herbstluftwm/
XSESSIONSDIR = $(SHAREDIR)/xsessions
ZSHCOMPLETIONDIR = $(SHAREDIR)/zsh/functions/Completion/X/
TARFILE = herbstluftwm-$(VERSION_MAJOR).$(VERSION_MINOR)$(VERSION_SUFFIX).tar.gz
A2X = a2x
ASCIIDOC = asciidoc
TMPTARDIR = herbstluftwm-$(VERSION_MAJOR).$(VERSION_MINOR)$(VERSION_SUFFIX)
MKDIR = mkdir -p
INSTALL = install

# Controls verbose build
# Remove the @ to see the actual compiler invocations
VERBOSE = @
