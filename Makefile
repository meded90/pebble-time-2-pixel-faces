FACES := mosaic-grid flip-board info-tiles codex-weekly starry-digits meded90
APPS := voice-drop
PROJECTS := $(FACES) $(APPS)
PEBBLE ?= pebble

.PHONY: build clean $(PROJECTS)

build: $(PROJECTS)

$(PROJECTS):
	cd $@ && $(PEBBLE) build

clean:
	for project in $(PROJECTS); do (cd $$project && $(PEBBLE) clean); done
