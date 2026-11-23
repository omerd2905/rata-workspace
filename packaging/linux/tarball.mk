# Install / uninstall Rata Workspace from a Linux release tarball
PREFIX ?= $(HOME)/.local

.PHONY: install uninstall

install:
	install -d "$(PREFIX)/bin"
	install -d "$(PREFIX)/share/applications"
	install -d "$(PREFIX)/share/icons/hicolor/256x256/apps"
	install -d "$(PREFIX)/share/icons/hicolor/scalable/apps"
	install -d "$(PREFIX)/share/licenses/rata-workspace"
	install -m 755 bin/rata-workspace "$(PREFIX)/bin/rata-workspace"
	install -m 644 share/applications/rata-workspace.desktop "$(PREFIX)/share/applications/rata-workspace.desktop"
	install -m 644 share/icons/hicolor/256x256/apps/rata-workspace.png "$(PREFIX)/share/icons/hicolor/256x256/apps/rata-workspace.png"
	install -m 644 share/icons/hicolor/scalable/apps/rata-workspace.svg "$(PREFIX)/share/icons/hicolor/scalable/apps/rata-workspace.svg"
	cp -r share/licenses/rata-workspace/. "$(PREFIX)/share/licenses/rata-workspace/"
	@echo "Installed Rata Workspace to $(PREFIX). Ensure $(PREFIX)/bin is on your PATH."

uninstall:
	rm -f "$(PREFIX)/bin/rata-workspace"
	rm -f "$(PREFIX)/share/applications/rata-workspace.desktop"
	rm -f "$(PREFIX)/share/icons/hicolor/256x256/apps/rata-workspace.png"
	rm -f "$(PREFIX)/share/icons/hicolor/scalable/apps/rata-workspace.svg"
	rm -rf "$(PREFIX)/share/licenses/rata-workspace"
	@echo "Uninstalled Rata Workspace from $(PREFIX). User settings/projects were not touched."
