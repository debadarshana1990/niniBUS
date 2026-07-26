APP_NAME := niniBUS Visualizer
APP_BUNDLE := $(APP_NAME).app
CONTENTS := $(APP_BUNDLE)/Contents
MACOS := $(CONTENTS)/MacOS

.PHONY: all app clean run

all: app

app: NiniBUSVisualizer.swift Info.plist
	mkdir -p "$(MACOS)"
	cp Info.plist "$(CONTENTS)/Info.plist"
	swiftc -parse-as-library -O -framework SwiftUI -framework AppKit \
		NiniBUSVisualizer.swift -o "$(MACOS)/$(APP_NAME)"
	codesign --force --deep --sign - "$(APP_BUNDLE)"

run: app
	open "$(APP_BUNDLE)"

clean:
	rm -rf "$(APP_BUNDLE)"
