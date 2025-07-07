[![CMake workflow](https://github.com/LegalizeAdulthood/tgautils/actions/workflows/cmake.yml/badge.svg)](https://github.com/LegalizeAdulthood/tgautils/actions/workflows/cmake.yml)

# Truevision TGA Image File Library

This is an update of the TGAUTILS.ZIP code for MS-DOS.  It was extended to extract
common code from the utilities for reading and writing TGA files into a library.

NOTE: PC byte ordering is assumed and no byte swapping is performed for machines
with a different byte endianness.

## Documentation

- [Truevision TGA file format](docs/tga-spec.pdf), version 2.2 [PDF]
- [TGA utilities](docs/tgautils.txt)

# Obtaining the Source

Use git to clone this repository, then update the vcpkg submodule to bootstrap
the dependency process.

```
git clone https://github.com/LegalizeAdulthood/tgautils
cd tgautils
```

# Building

A CMake preset has been provided to perform the usual CMake steps of
configure, build and test.

```
cmake --workflow --preset default
```

Places the build outputs in a sibling directory of the source code directory, e.g. up
and outside of the source directory.
