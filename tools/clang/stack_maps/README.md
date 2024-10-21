Prototype and playground for generating stack maps to garbage-collected objects
using clang/llvm infrastructure.

Design doc: https://bit.ly/chromium-stack-maps

Building and Running Tests:

1. Build the gc libary

`mkdir gc/build/ && cd gc/build`

`cmake ../ && make`

2. Build the LLVM passes:

`mkdir ../../build/ && cd ../../build/`

`cmake ../ && make all`

3. Run the tests (from stack_maps/tests/)

`./test.py <path_to_chromium_llvm_bin_dir> ../gc/build/libGC.a \
../build/IdentifySafepoints/libLLVMIdentifySafepointsPass.so \
../build/RegisterGcFunctionsPass.so`
