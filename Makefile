
# project
SRCDIR = src
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = ${SRC:.c=.o}
TARGET = herbstluftwm

# environment
LD = gcc
CC = gcc
CFLAGS = -pedantic -Wall -Werror

# colors
COLOR_ACTION  = "\e[1;33m"
COLOR_FILE    = "\e[1;32m"
COLOR_BRACKET = "\e[0;34m"
COLOR_NORMAL  = "\e[0m"
define colorecho
	@echo -e $(COLOR_BRACKET)"  ["$(COLOR_ACTION)$1$(COLOR_BRACKET)"]  " $(COLOR_FILE)$2$(COLOR_BRACKET)... $(COLOR_NORMAL)
endef


all: $(TARGET)

rb: clean all

$(TARGET): $(OBJ)
	$(call colorecho,LD,$(TARGET))
	@$(LD) -o $@ $(LDFLAGS)  $(OBJ)


$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(call colorecho,CC,$<)
	@$(CC) -c $(CFLAGS) -o $@ $<


clean:
	$(call colorecho,RM,$(TARGET))
	@rm -f $(TARGET)
	$(call colorecho,RM,$(OBJ))
	@rm -f $(OBJ)

.PHONY: all clean rb

