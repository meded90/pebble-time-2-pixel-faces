FACES := mosaic-grid flip-board info-tiles
PEBBLE ?= pebble

.PHONY: build clean $(FACES)

build: $(FACES)

$(FACES):
	cd $@ && $(PEBBLE) build

clean:
	for face in $(FACES); do (cd $$face && $(PEBBLE) clean); done
