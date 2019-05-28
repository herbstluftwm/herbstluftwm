# version is major.minor.patchlevel$suffix

VERSION_MAJOR = 0
VERSION_MINOR = 7
# patch level
VERSION_PATCH = 2
# git version
ifneq (,$(wildcard .git))
ifneq (,$(shell which git 2>/dev/null))
VERSION_GIT = \ \($(shell git rev-parse --short HEAD)\)
endif
endif
VERSION_SUFFIX = ""
SHORTVERSION = $(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)$(VERSION_SUFFIX)
VERSION = $(SHORTVERSION)$(VERSION_GIT)
