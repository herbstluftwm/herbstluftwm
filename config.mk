
# paths
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama
XINERAMALIBS = -L${X11LIB} -lXinerama
XINERAMAFLAGS = -DXINERAMA

INCS = -Isrc/ -I/usr/include -I${X11INC}  `pkg-config --cflags glib-2.0`
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11 $(XINERAMALIBS) `pkg-config --libs glib-2.0`

ifeq ($(shell uname),Linux)
LIBS += -lrt
endif

# FLAGS
CC ?= gcc
LD = $(CC)
CFLAGS ?= -g -pedantic -Wall
CFLAGS += -std=c99 ${INCS} -D _XOPEN_SOURCE=600
CFLAGS += $(CPPFLAGS)
VERSIONFLAGS = \
    -D HERBSTLUFT_VERSION=$(VERSION) \
    -D HERBSTLUFT_VERSION_MAJOR=$(VERSION_MAJOR) \
    -D HERBSTLUFT_VERSION_MINOR=$(VERSION_MINOR) \
    -D HERBSTLUFT_VERSION_PATCH=$(VERSION_PATCH)
CFLAGS += $(VERSIONFLAGS)
CFLAGS += $(XINERAMAFLAGS)
CFLAGS += -D HERBSTLUFT_GLOBAL_AUTOSTART=\"$(CONFIGDIR)/autostart\"
LDFLAGS ?= -g
DESTDIR = /
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share
MANDIR = $(DATADIR)/man
MAN1DIR = $(MANDIR)/man1
DOCDIR = $(DATADIR)/doc/herbstluftwm
EXAMPLESDIR = $(DOCDIR)/examples
LICENSEDIR = $(DOCDIR)
SYSCONFDIR = /etc
CONFIGDIR = $(SYSCONFDIR)/xdg/herbstluftwm
XSESSIONSDIR = $(DATADIR)/xsessions
ZSHCOMPLETIONDIR = $(DATADIR)/zsh/functions/Completion/X/
BASHCOMPLETIONDIR = $(SYSCONFDIR)/bash_completion.d/
TARFILE = herbstluftwm-$(VERSION_MAJOR).$(VERSION_MINOR)$(VERSION_SUFFIX).tar.gz
A2X = a2x
ASCIIDOC = asciidoc
TMPTARDIR = herbstluftwm-$(VERSION_MAJOR).$(VERSION_MINOR)$(VERSION_SUFFIX)
MKDIR = mkdir -p
INSTALL = install

# Controls verbose build
# Remove the @ to see the actual compiler invocations
VERBOSE = @

# Set this to 0 to disable colors
COLOR = 1
