include version.mk
include config.mk
include colors.mk

HLWMSRC = $(wildcard src/*.cpp)
HLWMOBJ = $(HLWMSRC:.cpp=.o)
HLWMTARGET = herbstluftwm

HCSRC = $(wildcard ipc-client/*.c)
HCOBJ = $(HCSRC:.c=.o)
HCTARGET = herbstclient

TARGETS = $(HLWMTARGET) $(HCTARGET)
OBJ = $(HLWMOBJ) $(HCOBJ)
DEPS = $(OBJ:.o=.d)

HERBSTCLIENTDOC = doc/herbstclient.txt
HERBSTLUFTWMDOC = doc/herbstluftwm.txt
TUTORIAL = doc/herbstluftwm-tutorial.txt

.PHONY: depend all all-nodoc doc install install-nodoc info www
.PHONY: cleandoc cleanwww cleandeps clean

all: $(TARGETS) doc
all-nodoc: $(TARGETS)

$(HCTARGET): $(HCOBJ)
	$(call colorecho,LD,$@)
	$(VERBOSE) $(LD) -o $@ $(LDFLAGS) $^ $(LIBS)

$(HLWMTARGET): $(HLWMOBJ)
	$(call colorecho,LDXX,$@)
	$(VERBOSE) $(LDXX) -o $@ $(LDXXFLAGS) $^ $(LIBS)

-include $(DEPS)

%.o: %.c version.mk
	$(call colorecho,CC,$<)
	$(VERBOSE) $(CC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<
	$(VERBOSE) $(CC) -c $(CPPFLAGS) -o $*.d -MT $@ -MM $<

%.o: %.cpp version.mk
	$(call colorecho,CXX,$<)
	$(VERBOSE) $(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $@ $<
	$(VERBOSE) $(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o $*.d -MT $@ -MM $<


info:
	@echo Some Info:
	@echo Preprocessing with: $(CC) -E $(CPPFLAGS)
	@echo Compiling C with: $(CC) -c $(CPPFLAGS) $(CFLAGS) -o OUT INPUT
	@echo Compiling C++ with: $(CXX) -c $(CPPFLAGS) $(CXXFLAGS) -o OUT INPUT
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
	$(call colorecho,RM,doc/herbstluftwm-tutorial.7)
	$(VERBOSE) rm -f doc/herbstluftwm-tutorial.7
	$(call colorecho,RM,doc/herbstluftwm-tutorial.html)
	$(VERBOSE) rm -f doc/herbstluftwm-tutorial.html

doc: doc/herbstclient.1    \
     doc/herbstclient.html \
     doc/herbstluftwm.1    \
     doc/herbstluftwm.html \
     doc/herbstluftwm-tutorial.7    \
     doc/herbstluftwm-tutorial.html

tar: doc
	tar -czf $(TARFILE) `git ls-files` doc/*.html doc/*.[0-9]
	rm -rf $(TMPTARDIR)
	mkdir -p $(TMPTARDIR)
	tar -xvf $(TARFILE) -C $(TMPTARDIR)
	tar -czf $(TARFILE) $(TMPTARDIR)
	rm -rf $(TMPTARDIR)
	gpg --detach-sign $(TARFILE)

doc/%.1 doc/%.7: doc/%.txt version.mk
	$(call colorecho,DOC,$@)
	$(VERBOSE) $(A2X) -f manpage -a "herbstluftwmversion=herbstluftwm $(VERSION)" -a "date=`date +%Y-%m-%d`" $<

doc/%.html: doc/%.txt version.mk
	$(call colorecho,DOC,$@)
	$(VERBOSE) $(ASCIIDOC) $<

install: all install-nodoc
	@echo "==> creating dirs..."
	$(MKDIR) '$(DESTDIR)$(MAN1DIR)'
	$(MKDIR) '$(DESTDIR)$(MAN7DIR)'
	$(MKDIR) '$(DESTDIR)$(DOCDIR)'
	@echo "==> copying files..."
	$(INSTALL) -m 644 doc/herbstclient.1 '$(DESTDIR)$(MAN1DIR)/'
	$(INSTALL) -m 644 doc/herbstluftwm.1 '$(DESTDIR)$(MAN1DIR)/'
	$(INSTALL) -m 644 doc/herbstluftwm-tutorial.7 '$(DESTDIR)$(MAN7DIR)/'
	$(INSTALL) -m 644 doc/herbstclient.html '$(DESTDIR)$(DOCDIR)/'
	$(INSTALL) -m 644 doc/herbstluftwm.html '$(DESTDIR)$(DOCDIR)/'
	$(INSTALL) -m 644 doc/herbstluftwm-tutorial.html '$(DESTDIR)$(DOCDIR)/'

install-nodoc: all-nodoc
	@echo "==> creating dirs..."
	$(MKDIR) '$(DESTDIR)$(LICENSEDIR)'
	$(MKDIR) '$(DESTDIR)$(BINDIR)'
	$(MKDIR) '$(DESTDIR)$(DOCDIR)'
	$(MKDIR) '$(DESTDIR)$(EXAMPLESDIR)'
	$(MKDIR) '$(DESTDIR)$(BASHCOMPLETIONDIR)'
	$(MKDIR) '$(DESTDIR)$(CONFIGDIR)'
	$(MKDIR) '$(DESTDIR)$(ZSHCOMPLETIONDIR)'
	$(MKDIR) '$(DESTDIR)$(XSESSIONSDIR)'
	@echo "==> copying files..."
	$(INSTALL) $(TARGETS) '$(DESTDIR)$(BINDIR)/'
	$(INSTALL) -m 644 LICENSE '$(DESTDIR)$(LICENSEDIR)/'
	$(INSTALL) -m 644 BUGS '$(DESTDIR)$(DOCDIR)/'
	$(INSTALL) -m 644 NEWS '$(DESTDIR)$(DOCDIR)/'
	$(INSTALL) -m 644 INSTALL '$(DESTDIR)$(DOCDIR)/'
	$(INSTALL) -m 755 share/autostart '$(DESTDIR)$(CONFIGDIR)/'
	$(INSTALL) -m 755 share/panel.sh '$(DESTDIR)$(CONFIGDIR)/'
	$(INSTALL) -m 755 share/restartpanels.sh '$(DESTDIR)$(CONFIGDIR)/'
	$(INSTALL) -m 644 share/herbstclient-completion '$(DESTDIR)$(BASHCOMPLETIONDIR)/'
	$(INSTALL) -m 644 share/_herbstclient '$(DESTDIR)$(ZSHCOMPLETIONDIR)/'
	$(INSTALL) -m 644 share/herbstluftwm.desktop '$(DESTDIR)$(XSESSIONSDIR)/'
	$(INSTALL) -m 755 share/dmenu_run_hlwm '$(DESTDIR)$(BINDIR)/'
	$(INSTALL) -m 644 scripts/README '$(DESTDIR)$(EXAMPLESDIR)/'
	$(INSTALL) -m 755 scripts/*.sh '$(DESTDIR)$(EXAMPLESDIR)/'



uninstall:
	@echo "==> deleting files..."
	-$(foreach TARGET,$(TARGETS),$(RM) '$(DESTDIR)$(BINDIR)/$(TARGET)';)
	-$(RM) '$(DESTDIR)$(BINDIR)/dmenu_run_hlwm'
	-$(RM) '$(DESTDIR)$(LICENSEDIR)/LICENSE'
	-$(RM) '$(DESTDIR)$(MAN1DIR)/herbstclient.1'
	-$(RM) '$(DESTDIR)$(MAN1DIR)/herbstluftwm.1'
	-$(RM) '$(DESTDIR)$(MAN7DIR)/herbstluftwm-tutorial.7'
	-$(RM) '$(DESTDIR)$(DOCDIR)/herbstclient.html'
	-$(RM) '$(DESTDIR)$(DOCDIR)/herbstluftwm.html'
	-$(RM) '$(DESTDIR)$(DOCDIR)/herbstluftwm-tutorial.html'
	-$(RM) '$(DESTDIR)$(DOCDIR)/BUGS'
	-$(RM) '$(DESTDIR)$(DOCDIR)/NEWS'
	-$(RM) '$(DESTDIR)$(DOCDIR)/INSTALL'
	-$(RM) '$(DESTDIR)$(CONFIGDIR)/autostart'
	-$(RM) '$(DESTDIR)$(CONFIGDIR)/panel.sh'
	-$(RM) '$(DESTDIR)$(CONFIGDIR)/restartpanels.sh'
	-$(RM) '$(DESTDIR)$(BASHCOMPLETIONDIR)/herbstclient-completion'
	-$(RM) '$(DESTDIR)$(ZSHCOMPLETIONDIR)/_herbstclient'
	-$(RM) '$(DESTDIR)$(XSESSIONSDIR)/herbstluftwm.desktop'
	-$(RM) '$(DESTDIR)$(EXAMPLESDIR)/README'
	-$(RM) '$(DESTDIR)$(EXAMPLESDIR)'/*.sh
	@echo "==> deleting directories..."
	-$(RMDIR) '$(DESTDIR)$(EXAMPLESDIR)/'
	-$(RMDIR) '$(DESTDIR)$(DOCDIR)/'
	-$(RMDIR) '$(DESTDIR)$(CONFIGDIR)/'

www:
	make -C www

cleanwww:
	make -C www clean
