FACES := mosaic-grid flip-board info-tiles codex-weekly starry-digits meded90 zodiac-aquarius zodiac-gemini
APPS := voice-drop
PROJECTS := $(FACES) $(APPS)
PEBBLE ?= pebble

.PHONY: build apps build-all clean $(PROJECTS)

build: $(FACES)

apps: $(APPS)

build-all: $(PROJECTS)

$(PROJECTS):
	cd $@ && $(PEBBLE) build

clean:
	for project in $(PROJECTS); do (cd $$project && $(PEBBLE) clean); done
