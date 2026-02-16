# Wrapper around the qmake-generated Makefile that adds a docs target.
# GNU make prefers GNUmakefile over Makefile, so this is picked up
# automatically while the qmake Makefile is left untouched.

SPHINXBUILD   ?= sphinx-build
SPHINXOPTS    ?= -q
DOCDIR        = doc
BUILDDIR      = $(DOCDIR)/_build

.DEFAULT_GOAL := all

all: paperman paperman-server app docs

# Targets handled here
.PHONY: builddate.h
builddate.h:
	@echo '#define SERVER_BUILD_DATE "$(BUILD_DATE)"' > $@

paperman-server: Makefile.server builddate.h
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

BUILD_DATE := $(shell date "+%a %d %b %H:%M:%S %Z %Y")
APP_VERSION := $(shell sed -n 's/.*CONFIG_version_str "\(.*\)"/\1.0/p' config.h)
FLUTTER_ARGS = --build-name=$(APP_VERSION) '--dart-define=BUILD_DATE=$(BUILD_DATE)'

app: app-apk app-linux

app-apk:
	cd app && flutter build apk $(FLUTTER_ARGS)

app-aab:
	cd app && flutter build appbundle $(FLUTTER_ARGS)

app-publish: app-aab
	cd app/android && ./gradlew publishReleaseBundle

app-upload: app-apk
	rclone copy $(APP_APK) gdrive:apps/

app-linux:
	cd app && flutter build linux $(FLUTTER_ARGS)

APP_APK  = app/build/app/outputs/flutter-apk/app-release.apk
APP_AAB  = app/build/app/outputs/bundle/release/app-release.aab
APP_BIN  = app/build/linux/x64/release/bundle/paperman

info:
	@ls -l paperman paperman-server $(APP_APK) $(APP_AAB) $(APP_BIN) 2>/dev/null || echo "No binaries found (run 'make' first)"

help:
	@echo "Build targets:"
	@echo "  all              Build everything (default)"
	@echo "  paperman         Build the Qt desktop app"
	@echo "  paperman-server  Build the standalone server"
	@echo "  app              Build the Flutter app (Android + Linux)"
	@echo "  app-apk          Build the Flutter Android APK only"
	@echo "  app-aab          Build the Flutter Android App Bundle only"
	@echo "  app-publish      Build AAB and upload to Play Store internal testing"
	@echo "  app-upload       Build APK and upload to Google Drive"
	@echo "  app-linux        Build the Flutter Linux binary only"
	@echo "  docs             Build the Sphinx documentation"
	@echo ""
	@echo "Test targets:"
	@echo "  test             Run all tests"
	@echo "  test-progressive Run progressive-loading tests"
	@echo "  test-parallel    Run parallel tests"
	@echo ""
	@echo "Other targets:"
	@echo "  info             List built binaries"
	@echo "  clean            Clean all build outputs"
	@echo "  app-clean        Clean Flutter build outputs"
	@echo "  docs-clean       Clean Sphinx build outputs"

app-clean:
	-cd app && flutter clean

clean: app-clean docs-clean
	$(MAKE) -f Makefile clean
	rm -f builddate.h

docs-clean:
	rm -rf $(BUILDDIR)

# Forward everything else to the qmake-generated Makefile
%:
	$(MAKE) -f Makefile $@
