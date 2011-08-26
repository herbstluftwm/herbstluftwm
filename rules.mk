
# colors
TPUT = tput
COLOR_CLEAR   = `$(TPUT) sgr0`
COLOR_NORMAL  = $(COLOR_CLEAR)
COLOR_ACTION  = `$(TPUT) bold``$(TPUT) setaf 3`
COLOR_FILE    = `$(TPUT) bold``$(TPUT) setaf 2`
COLOR_BRACKET = $(COLOR_CLEAR)`$(TPUT) setaf 4`
define colorecho
	@echo $(COLOR_BRACKET)"  ["$(COLOR_ACTION)$1$(COLOR_BRACKET)"]  " $(COLOR_FILE)$2$(COLOR_BRACKET)... $(COLOR_NORMAL)
endef


all: $(TARGET)

rb: clean all

$(TARGET): $(OBJ)
	$(call colorecho,LD,$(TARGET))
	@$(LD) -o $@ $(LDFLAGS)  $(OBJ)

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(HEADER)
	$(call colorecho,CC,$<)
	@$(CC) -c $(CFLAGS) -o $@ $<

clean:
	$(call colorecho,RM,$(TARGET))
	@rm -f $(TARGET)
	$(call colorecho,RM,$(OBJ))
	@rm -f $(OBJ)

info:
	@echo Some Info:
	@echo Compiling with: $(CC) -c $(CFLAGS) -o OUT INPUT
	@echo Linking with: $(LD) -o OUT $(LDFLAGS) INPUT

.PHONY: all clean rb info

