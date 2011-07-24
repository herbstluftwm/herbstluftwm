
include config.mk

# project
SRCDIR = src
SRC = $(wildcard $(SRCDIR)/*.c)
HEADER = $(wildcard $(SRCDIR)/*.h)
OBJ = ${SRC:.c=.o}
TARGET = herbstluftwm

include rules.mk

all: build-herbstclient
clean: clean-herbstclient

build-herbstclient:
	make -C ipc-client

clean-herbstclient:
	make -C ipc-client clean

