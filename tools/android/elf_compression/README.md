# Shared library compression
## Description
This directory contains the shared library compression tool that allows to
reduce the shared library size by compressing non-performance critical code.
The decompression is done on demand by a watcher thread that is set in a
library constructor. This constructor (located in a single .c file) should be
included in the library's build.

Additional information may be found in the tracking bug:
https://crbug.com/998082

The tool consists of 2 parts:
### Compression script
This script does the compression part. It removes the specified range from
the file and adds compressed version of it instead. It then sets decompression
hook's smagic bytes to point at the cutted range and to the compressed
version.
### Decompression hook
Located at `decompression_hook/` path and should be build together with the target
library.

It decompresses data from compressed section, provided by compression script
and populates the target range by setting a new library constructor which
starts a watcher thread, handling page fault events.

## Usage
Firstly, the library needs to be build with the tool's decompression hook. To
do this add the following file to your build:

    decompression_hook/decompression_hook.c

Additionally the library must be linked with `-pthread` option.

After the library build is complete, the compression script must be applied to
it in the following way:

    ./compress_section.py -i lib.so -o patched_lib.so -l <begin> -r <end>

Where `<begin>` and `<end>` are the file offsets of the part that you want to
compress.

It is important to note that after running the script some of the common ELF
tooling utilities, for example objcopy, may stop working because of the
unusual (but legal) changes made to the library.

## Testing
To run tests:

    test/run_tests.py

