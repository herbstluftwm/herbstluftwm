INCS = -Isrc/ -I/usr/include -I${X11INC}
LIBS = -lc -L${X11LIB} -lXext -lX11

CFLAGS += -g -Wall -Wextra -std=c99 ${INCS}
LDFLAGS += ${LIBS}

BIN = $(patsubst %.c,%,$(wildcard *.c))

.PHONY: all clean .gitignore

all: $(BIN)

clean:
	rm -f $(BIN)

.gitignore:
	printf "%s\n" $(BIN) | tee .gitignore

