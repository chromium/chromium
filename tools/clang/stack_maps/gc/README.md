# Stack Map Artefact GC

This directory contains the implementation of the runtime system necessary to
observe and test the LLVM generated stack maps.

## GC API

Provides a few useful function calls  which can be made from user-code.

## Stack Map Parser

The Stack Map parser parses the `.llvm_stackmaps` section according to the LLVM
V3 StackMap format. It begins parsing from the global `__LLVM_StackMaps` symbol.
The parser then builds a map from this data which can queried by the stack
walker at a later stage to identify the garbage collection rootset.
