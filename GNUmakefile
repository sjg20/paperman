# Wrapper around the qmake-generated Makefile that adds a docs target.
# GNU make prefers GNUmakefile over Makefile, so this is picked up
# automatically while the qmake Makefile is left untouched.

SPHINXBUILD   ?= sphinx-build
SPHINXOPTS    ?= -q
DOCDIR        = doc
BUILDDIR      = $(DOCDIR)/_build

.DEFAULT_GOAL := all

all: paperman app docs

# Targets handled here
paperman-server: Makefile.server
	$(MAKE) -f Makefile.server

Makefile.server: paperman-server.pro
	qmake paperman-server.pro -o Makefile.server

test: paperman-server paperman
	scripts/test_page_fetch.sh
	scripts/test_parallel.sh

test-progressive: paperman-server
	scripts/test_progressive.sh

test-parallel: paperman
	scripts/test_parallel.sh

docs:
	$(SPHINXBUILD) -b html $(SPHINXOPTS) $(DOCDIR) $(BUILDDIR)/html

app:
	cd app && flutter build linux --dart-define=BUILD_DATE=$(shell date "+%Y-%m-%d %H:%M")

app-clean:
	cd app && flutter clean

docs-clean:
	rm -rf $(BUILDDIR)

# Forward everything else to the qmake-generated Makefile
%:
	$(MAKE) -f Makefile $@
