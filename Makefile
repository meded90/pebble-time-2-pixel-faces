# projects:make:start
FACES := mosaic-grid flip-board info-tiles codex-weekly starry-digits meded90 zodiac-aquarius zodiac-gemini
STANDARD_APPS := gym-zones wrist-agent find-my-iphone
PATCHED_APPS := voice-drop
# projects:make:end
APPS := $(STANDARD_APPS) $(PATCHED_APPS)
PROJECTS := $(FACES) $(APPS)
PEBBLE ?= pebble

.PHONY: build standard-apps patched-apps apps build-all clean $(PROJECTS)

build: $(FACES)

standard-apps: $(STANDARD_APPS)

patched-apps: $(PATCHED_APPS)

apps: standard-apps patched-apps

build-all: $(PROJECTS)

$(PROJECTS):
	cd $@ && $(PEBBLE) build

clean:
	for project in $(PROJECTS); do (cd $$project && $(PEBBLE) clean); done
