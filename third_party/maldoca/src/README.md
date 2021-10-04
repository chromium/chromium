# MalDocA - Malicious Document Analyzer

MalDocA is a library to parse and extract features from Microsoft Office documents. It supports both OLE and OOXML documents.

The project's goal is to analyze potentially malicious documents to improve user safety and security.

## REQUIREMENTS
- [Bazel](https://bazel.build) (recommended version: 4)
- [Clang](https://clang.llvm.org) (recommended version: 11 or 12)
- OS: Linux or Windows

## GENERAL
Some testdata files contain malicious code! Hence, we use a xor-encoding for some testdata files as a safety measure (key = 0x42). Therefore, be very careful when opening / processing them!

For convenience, we provide a python script (["testdata_encode.py"](https://github.com/google/maldoca/testdata_encode.py)) to encode / decode those files. The script's output is stored in the same path, having "_xored" as file name appendix. Keep in mind that encoding a file twice decodes it again, i.e. restores the original file.

Example usage: python testdata_encode.py maldoca/service/testdata/c98661bcd5bd2e5df06d3432890e7a2e8d6a3edcb5f89f6aaa2e5c79d4619f3d.docx

## WINDOWS
- Bazel has some Windows related problems, e.g. maximum path length limitations. Make sure to read the [best-practices](https://docs.bazel.build/versions/main/windows.html#best-practices) to avoid them.
- Enable symlink support ([how-to](https://docs.bazel.build/versions/main/windows.html#enable-symlink-support)) as it is required by Bazel.

## CHECKOUT
git clone --recurse-submodules https://github.com/google/maldoca.git

cd maldoca

## BUILD
Linux: bazel build --config=linux //maldoca/...

Windows: bazel build --config=windows //maldoca/...

## TEST
Linux: bazel test --config=linux //maldoca/...

Windows: bazel test --config=windows //maldoca/...

## DOCKER
We provide a docker file in "docker/Dockerfile". This is the reference
platform we use for continuous integration and optionally (arguably recommended)
for development as well. Please check the documentation in "docker/Dockerfile" on how to
build and use for development.

## CONTACT
maldoca-community@google.com

## DISCLAIMER
This is not an official Google product.
