
# paths
X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11

# FLAGS
LD = gcc
CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -Werror
LDFLAGS = -g ${LIBS}

