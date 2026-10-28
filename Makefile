# Builds PBookOrbit with pocketbook-kit: the kit/ submodule if present, otherwise pb from
# your PATH. PB_KIT=/path/to/pocketbook-kit uses another checkout.
PB_KIT ?= kit
PB := $(if $(wildcard $(PB_KIT)/pb.sh),$(PB_KIT)/pb.sh,pb)

.PHONY: setup run run-offline pc arm release shell clean icons

# Fetch the kit, its SDK and the container image
setup:
	git submodule update --init --recursive
	$(PB) setup

# Build for PC and open the app in the emulator
run:
	$(PB) run .

# Same, acting as though there were no network (see Offline in docs/technical.md)
run-offline:
	PB_ENV="PBOOKORBIT_OFFLINE=1 $(PB_ENV)" $(PB) run .

# Build for PC only
pc:
	$(PB) pc .

# Build for the device; copy build/pbookorbit/<sdk>/arm/pbookorbit.app to its applications/
# folder
arm:
	$(PB) arm .

# Device build with the sign-in server prefilled, for users of that server: make release
# SERVER=books.example.com
release:
	$(if $(SERVER),,$(error set SERVER, e.g. make release SERVER=books.example.com))
	PB_QMAKE_ARGS="SUGGESTED_SERVER=$(SERVER)" $(PB) arm .

# Render the icons built into the app (src/icons/*.bmp) from their SVG sources as 110x110
# 8-bit grey BMPs, the Era Color's theme icon size. Needs rsvg-convert and ImageMagick.
icons:
	for svg in src/icons/*.svg; do \
		rsvg-convert "$$svg" | magick - -colorspace Gray -type Palette -depth 8 -compress None "BMP3:$${svg%.svg}.bmp" || exit 1; \
	done

# Shell in the build container
shell:
	$(PB) shell .

clean:
	$(PB) clean .
