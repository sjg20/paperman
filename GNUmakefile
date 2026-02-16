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

test-setup: paperman
	python3 scripts/make_test_files.py

app-test:
	cd app && flutter test

test: paperman-server paperman test-setup app-test
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
DART_DEFINES = app/dart-defines.json
FLUTTER_ARGS = --build-name=$(APP_VERSION) --dart-define-from-file=dart-defines.json

app: app-apk app-linux

.PHONY: app app-demo app-test dart-defines app-apk app-aab app-publish
.PHONY: app-upload app-scp app-scp-only app-linux app-clean
dart-defines:
	@echo '{"BUILD_DATE":"$(BUILD_DATE)"}' > $(DART_DEFINES)

app-demo:
	python3 app/tools/gen_demo_assets.py

app-apk: app-demo dart-defines
	cd app && flutter build apk $(FLUTTER_ARGS)

app-aab: app-demo dart-defines
	cd app && flutter build appbundle $(FLUTTER_ARGS)

app-publish: app-aab
	cd app/android && ./gradlew publishReleaseBundle

app-upload: app-apk
	rclone copy $(APP_APK) gdrive:apps/

-include server.mk

app-scp: app-apk app-scp-only

app-scp-only:
	@test -n "$(APP_SERVER)" || { echo "Set APP_SERVER in server.mk (e.g. APP_SERVER = user@host:/var/www/app.apk)"; exit 1; }
	scp $(APP_APK) $(APP_SERVER)

app-linux: dart-defines
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
	@echo "  app-scp          Build APK and scp to server (needs server.mk)"
	@echo "  app-demo         Generate demo assets (PDFs + thumbnails)"
	@echo "  app-linux        Build the Flutter Linux binary only"
	@echo "  docs             Build the Sphinx documentation"
	@echo ""
	@echo "Test targets:"
	@echo "  test             Run all tests"
	@echo "  test-setup       Generate test files in test/files/"
	@echo "  test-progressive Run progressive-loading tests"
	@echo "  test-parallel    Run parallel tests"
	@echo "  app-test         Run Flutter widget tests"
	@echo ""
	@echo "Other targets:"
	@echo "  info             List built binaries"
	@echo "  clean            Clean all build outputs"
	@echo "  app-clean        Clean Flutter build outputs"
	@echo "  docs-clean       Clean Sphinx build outputs"

app-clean:
	-cd app && flutter clean
	-rm -rf app/build app/android/.gradle
	rm -f $(DART_DEFINES)

clean: app-clean docs-clean
	$(MAKE) -f Makefile clean
	rm -f builddate.h Makefile.server

docs-clean:
	rm -rf $(BUILDDIR)

# Forward everything else to the qmake-generated Makefile
%:
	$(MAKE) -f Makefile $@
