
# paths
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -Isrc/ -I/usr/include -I${X11INC}  `pkg-config --cflags glib-2.0`
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11 `pkg-config --libs glib-2.0`

# FLAGS
LD = gcc
CC = gcc
CFLAGS = -g -std=c99 -pedantic -Wall ${INCS}
LDFLAGS = -g ${LIBS}
PREFIX = ./build
BINDIR = $(PREFIX)/bin
SHAREDIR = $(PREFIX)/share
MANDIR = $(PREFIX)/man/man1
DOCDIR = $(PREFIX)/doc/herbstluftwm
ETCDIR = ./etc/
CONFIGDIR = $(ETCDIR)/xdg/herbstluftwm/

