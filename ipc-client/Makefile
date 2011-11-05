
include ../version.mk
include ../config.mk

# project
SRCDIR = .
SRC = $(wildcard $(SRCDIR)/*.c) ../src/utils.c
HEADER = $(wildcard $(SRCDIR)/*.h) ../src/ipc-protocol.h
OBJ = ${SRC:.c=.o}
TARGET = herbstclient

include ../colors.mk
include ../rules.mk

