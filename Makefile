# Builds PBookOrbit with pocketbook-kit: the kit/ submodule if there is one, else the pb
# command on your PATH. PB_KIT=/path/to/pocketbook-kit picks another checkout.
PB_KIT ?= kit
PB := $(if $(wildcard $(PB_KIT)/pb.sh),$(PB_KIT)/pb.sh,pb)

.PHONY: setup run pc arm release shell clean icons

# Fetch the kit, its SDK and the container image
setup:
	git submodule update --init --recursive
	$(PB) setup

# Build for PC and open the app in the emulator
run:
	$(PB) run .

# Build for PC only
pc:
	$(PB) pc .

# Build for the device: copy build/pbookorbit/<sdk>/arm/pbookorbit.app to its applications/ folder
arm:
	$(PB) arm .

# A device build with a server filled in on the sign-in page, to hand to someone who uses
# it: make release SERVER=books.example.com
release:
	$(if $(SERVER),,$(error set SERVER, e.g. make release SERVER=books.example.com))
	PB_QMAKE_ARGS="SUGGESTED_SERVER=$(SERVER)" $(PB) arm .

# Render the app's icons (src/icons/*.bmp, built into the app) from their line art
# (src/icons/*.svg): 110x110 8-bit grey BMPs, the size of the Era Color's theme icons.
# Needs rsvg-convert and ImageMagick.
icons:
	for svg in src/icons/*.svg; do \
		rsvg-convert "$$svg" | magick - -colorspace Gray -type Palette -depth 8 -compress None "BMP3:$${svg%.svg}.bmp" || exit 1; \
	done

# Shell in the build container
shell:
	$(PB) shell .

clean:
	$(PB) clean .
