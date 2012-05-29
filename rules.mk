
all: $(TARGET)

rb: clean all

$(TARGET): $(OBJ)
	$(call colorecho,LD,$(TARGET))
	$(VERBOSE) $(LD) -o $@ $(LDFLAGS)  $(OBJ) $(LIBS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(HEADER)
	$(call colorecho,CC,$<)
	$(VERBOSE) $(CC) -c $(CFLAGS) -o $@ $<

clean:
	$(call colorecho,RM,$(TARGET))
	$(VERBOSE) rm -f $(TARGET)
	$(call colorecho,RM,$(OBJ))
	$(VERBOSE) rm -f $(OBJ)

info:
	@echo Some Info:
	@echo Compiling with: $(CC) -c $(CFLAGS) -o OUT INPUT
	@echo Linking with: $(LD) -o OUT $(LDFLAGS) INPUT

.PHONY: all clean rb info

