# paths
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# Xinerama
XINERAMALIBS = `pkg-config --silence-errors --libs xinerama`
XINERAMAFLAGS = `pkg-config --exists xinerama && echo -DXINERAMA`

INCS = -Isrc/ -I/usr/include -I${X11INC}  `pkg-config --cflags glib-2.0`
LIBS = -lc -L${X11LIB} -lXext -lX11 $(XINERAMALIBS) `pkg-config --libs glib-2.0`

ifeq ($(shell uname),Linux)
LIBS += -lrt
endif

# FLAGS
CC ?= gcc
LD = $(CC)
LDXX ?= g++
CFLAGS ?= -g
CFLAGS += -pedantic -Wall -std=c99
CXXFLAGS ?= -g
CXXFLAGS += -pedantic -Wall -std=c++11 -Wno-sign-compare -Wno-narrowing -Wno-deprecated-register -Wno-invalid-offsetof
VERSIONFLAGS = \
    -D HERBSTLUFT_VERSION=\"$(VERSION)\" \
    -D HERBSTLUFT_VERSION_MAJOR=$(VERSION_MAJOR) \
    -D HERBSTLUFT_VERSION_MINOR=$(VERSION_MINOR) \
    -D HERBSTLUFT_VERSION_PATCH=$(VERSION_PATCH)
CPPFLAGS ?=
CPPFLAGS += $(INCS) -D _XOPEN_SOURCE=600 $(VERSIONFLAGS) $(XINERAMAFLAGS)
CPPFLAGS += -D HERBSTLUFT_GLOBAL_AUTOSTART=\"$(CONFIGDIR)/autostart\"
LDFLAGS ?= -g
DESTDIR =
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share
MANDIR = $(DATADIR)/man
MAN1DIR = $(MANDIR)/man1
MAN7DIR = $(MANDIR)/man7
DOCDIR = $(DATADIR)/doc/herbstluftwm
EXAMPLESDIR = $(DOCDIR)/examples
LICENSEDIR = $(DOCDIR)
SYSCONFDIR = /etc
CONFIGDIR = $(SYSCONFDIR)/xdg/herbstluftwm
XSESSIONSDIR = $(DATADIR)/xsessions
ZSHCOMPLETIONDIR = $(DATADIR)/zsh/functions/Completion/X
BASHCOMPLETIONDIR = $(SYSCONFDIR)/bash_completion.d
TARFILE = herbstluftwm-$(SHORTVERSION).tar.gz
A2X = a2x
ASCIIDOC = asciidoc
TMPTARDIR = herbstluftwm-$(SHORTVERSION)
MKDIR = mkdir -p
INSTALL = install
RM = rm -f
RMDIR = rmdir

# Controls verbose build
# Remove the @ to see the actual compiler invocations
VERBOSE = @

# Set this to 0 to disable colors
COLOR = 1
