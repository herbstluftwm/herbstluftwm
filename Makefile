
include config.mk

# project
SRCDIR = src
SRC = $(wildcard $(SRCDIR)/*.c)
HEADER = $(wildcard $(SRCDIR)/*.h)
OBJ = ${SRC:.c=.o}
TARGET = herbstluftwm

include rules.mk

