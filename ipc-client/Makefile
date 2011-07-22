
include ../config.mk

# project
SRCDIR = .
SRC = $(wildcard $(SRCDIR)/*.c)
HEADER = $(wildcard $(SRCDIR)/*.h) ../src/ipc-protocol.h
OBJ = ${SRC:.c=.o}
TARGET = herbstclient

include ../rules.mk

