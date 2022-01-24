all: overview.svg

build/bin/svgbob:
	cargo install --git https://github.com/ivanceras/svgbob --rev df01674c47350665158ececa476e63f51c58a9c7 --root build

%.svg: %.ascii build/bin/svgbob
	build/bin/svgbob $< > $@

clean:
	rm -f *.svg

.PHONY: all clean
