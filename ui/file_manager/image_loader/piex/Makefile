# Makefile for piex-wasm
#
# An IIFE https://developer.mozilla.org/en-US/docs/Glossary/IIFE is used to
# wrap em++ compiler-generated JS code so it does not pollute |globalThis|.
#
# CFI sanitizer options are used to compile our WebAssembly, as recommended
# by https://webassembly.org/docs/security/#memory-safety (for more details
# about CFI see https://clang.llvm.org/docs/ControlFlowIntegrity.html).
#
PIEX = node_modules/piex

PSRC = $(PIEX)/src/piex.cc \
       $(PIEX)/src/tiff_parser.cc \
       $(PIEX)/src/image_type_recognition/image_type_recognition_lite.cc \
       $(PIEX)/src/tiff_directory/tiff_directory.cc \
       $(PIEX)/src/binary_parse/range_checked_byte_ptr.cc \
       $(PIEX)/src/binary_parse/cached_paged_byte_array.cc

INCS = -I $(PIEX)
WASM = -s WASM=1 -fno-exceptions -Wall -fsanitize=cfi -flto -fvisibility=hidden
WOPT = -Os --llvm-opts 3 \
  -s STRICT=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s ENVIRONMENT='web' \
  -s NO_DYNAMIC_EXECUTION=1 \
  -s TOTAL_STACK=$(shell echo $$((8*1024)))

IIFE = --pre-js iife.beg --post-js iife.end

.PHONY: all clean release

all: piex.out.js release

piex.out.js: piex.cpp.bc piex.src.bc iife.beg iife.end
	em++ --bind --std=c++11 $(WASM) $(WOPT) $(IIFE) piex.cpp.bc piex.src.bc -o $@

piex.cpp.bc: piex.cpp Makefile
	em++ --bind --std=c++11 $(WASM) $(WOPT) $(INCS) piex.cpp -o $@

piex.src.bc: $(PSRC) Makefile
	em++ --bind --std=c++11 $(WASM) $(WOPT) $(INCS) $(PSRC) -o $@

release:
	$(shell cp piex.out.js piex.js.wasm)
	$(shell cp piex*wasm extension)

clean:
	$(shell rm -f tests.{result*,log} package-lock* *.bc piex.out.js)
	$(shell rm -f piex*wasm extension/piex*wasm)
