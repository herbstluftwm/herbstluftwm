
include version.mk
include config.mk
include colors.mk

HLWMSRC = $(wildcard src/*.c)
HLWMOBJ = $(HLWMSRC:.c=.o)
HLWMTARGET = herbstluftwm

HCSRC = $(wildcard ipc-client/*.c) src/utils.c
HCOBJ = $(HCSRC:.c=.o)
HCTARGET = herbstclient

TARGETS = $(HLWMTARGET) $(HCTARGET)
OBJ = $(HLWMOBJ) $(HCOBJ)
DEPS = $(OBJ:.o=.d)

HERBSTCLIENTDOC = doc/herbstclient.txt
HERBSTLUFTWMDOC = doc/herbstluftwm.txt

.PHONY: depend all all-nodoc doc install info www
.PHONY: cleandoc cleanwww cleandeps clean

all: $(TARGETS) doc
all-nodoc: $(TARGETS)
$(HLWMTARGET): $(HLWMOBJ)
$(HCTARGET): $(HCOBJ)

$(TARGETS):
	$(call colorecho,LD,$@)
	$(VERBOSE) $(LD) -o $@ $(LDFLAGS)  $^ $(LIBS)

-include $(DEPS)

%.o: %.c version.mk
	$(call colorecho,CC,$<)
	$(VERBOSE) $(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<
	$(VERBOSE) $(CC) -c $(CPPFLAGS) -o $*.d -MT $@ -MM $<

info:
	@echo Some Info:
	@echo Preprocessing with: $(CC) -E $(CPPFLAGS)
	@echo Compiling with: $(CC) -c $(CPPFLAGS) $(CFLAGS) -o OUT INPUT
	@echo Linking with: $(LD) -o OUT $(LDFLAGS) INPUT

clean: cleandoc cleandeps
	$(call colorecho,RM,$(TARGETS))
	$(VERBOSE) rm -f $(TARGETS)
	$(call colorecho,RM,$(OBJ))
	$(VERBOSE) rm -f $(OBJ)

cleandeps:
	$(call colorecho,RM,$(DEPS))
	$(VERBOSE) rm -f $(DEPS)

cleandoc:
	$(call colorecho,RM,doc/herbstclient.1)
	$(VERBOSE) rm -f doc/herbstclient.1
	$(call colorecho,RM,doc/herbstclient.html)
	$(VERBOSE) rm -f doc/herbstclient.html
	$(call colorecho,RM,doc/herbstluftwm.1)
	$(VERBOSE) rm -f doc/herbstluftwm.1
	$(call colorecho,RM,doc/herbstluftwm.html)
	$(VERBOSE) rm -f doc/herbstluftwm.html

doc: doc/herbstclient.1    \
     doc/herbstclient.html \
     doc/herbstluftwm.1    \
     doc/herbstluftwm.html

tar: doc
	tar -czf $(TARFILE) `git ls-files` doc/*.html doc/*.[0-9]
	rm -rf $(TMPTARDIR)
	mkdir -p $(TMPTARDIR)
	tar -xvf $(TARFILE) -C $(TMPTARDIR)
	tar -czf $(TARFILE) $(TMPTARDIR)
	rm -rf $(TMPTARDIR)

doc/%.1: doc/%.txt version.mk
	$(call colorecho,DOC,$@)
	$(VERBOSE) $(A2X) -f manpage -a "herbstluftwmversion=herbstluftwm $(VERSION)" -a "date=`date +%Y-%m-%d`" $<

doc/%.html: doc/%.txt version.mk
	$(call colorecho,DOC,$@)
	$(VERBOSE) $(ASCIIDOC) $<

install: all
	@echo "==> creating dirs..."
	$(MKDIR) '$(DESTDIR)/$(LICENSEDIR)'
	$(MKDIR) '$(DESTDIR)/$(BINDIR)'
	$(MKDIR) '$(DESTDIR)/$(MANDIR)/man1'
	$(MKDIR) '$(DESTDIR)/$(DOCDIR)'
	$(MKDIR) '$(DESTDIR)/$(EXAMPLESDIR)'
	$(MKDIR) '$(DESTDIR)/$(BASHCOMPLETIONDIR)'
	$(MKDIR) '$(DESTDIR)/$(CONFIGDIR)'
	$(MKDIR) '$(DESTDIR)/$(ZSHCOMPLETIONDIR)'
	$(MKDIR) '$(DESTDIR)/$(XSESSIONSDIR)'
	@echo "==> copying files..."
	$(INSTALL) $(TARGETS) '$(DESTDIR)/$(BINDIR)'
	$(INSTALL) -m 644 LICENSE '$(DESTDIR)/$(LICENSEDIR)'
	$(INSTALL) -m 644 doc/herbstclient.1 '$(DESTDIR)/$(MAN1DIR)/'
	$(INSTALL) -m 644 doc/herbstluftwm.1 '$(DESTDIR)/$(MAN1DIR)/'
	$(INSTALL) -m 644 doc/herbstclient.html '$(DESTDIR)/$(DOCDIR)/'
	$(INSTALL) -m 644 doc/herbstluftwm.html '$(DESTDIR)/$(DOCDIR)/'
	$(INSTALL) -m 644 BUGS '$(DESTDIR)/$(DOCDIR)/'
	$(INSTALL) -m 644 NEWS '$(DESTDIR)/$(DOCDIR)/'
	$(INSTALL) -m 644 README '$(DESTDIR)/$(DOCDIR)/'
	$(INSTALL) -m 755 share/autostart '$(DESTDIR)/$(CONFIGDIR)/'
	$(INSTALL) -m 755 share/panel.sh '$(DESTDIR)/$(CONFIGDIR)/'
	$(INSTALL) -m 755 share/restartpanels.sh '$(DESTDIR)/$(CONFIGDIR)/'
	$(INSTALL) -m 644 share/herbstclient-completion '$(DESTDIR)/$(BASHCOMPLETIONDIR)'
	$(INSTALL) -m 644 share/_herbstclient '$(DESTDIR)/$(ZSHCOMPLETIONDIR)/'
	$(INSTALL) -m 644 share/herbstluftwm.desktop '$(DESTDIR)/$(XSESSIONSDIR)/'
	$(INSTALL) -m 644 scripts/README '$(DESTDIR)/$(EXAMPLESDIR)/'
	$(INSTALL) -m 755 scripts/*.sh '$(DESTDIR)/$(EXAMPLESDIR)/'

www:
	make -C www

cleanwww:
	make -C www clean
