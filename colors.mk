# colors
ifeq ($(COLOR),1)
TPUT = tput
COLOR_CLEAR   = `$(TPUT) sgr0`
COLOR_NORMAL  = $(COLOR_CLEAR)
COLOR_ACTION  = `$(TPUT) bold``$(TPUT) setaf 3`
COLOR_FILE    = `$(TPUT) bold``$(TPUT) setaf 2`
COLOR_BRACKET = $(COLOR_CLEAR)`$(TPUT) setaf 4`
endif

define colorecho
	@echo $(COLOR_BRACKET)"  ["$(COLOR_ACTION)$1$(COLOR_BRACKET)"]  " $(COLOR_FILE)$2$(COLOR_BRACKET)... $(COLOR_NORMAL)
endef
