[![CMake workflow](https://github.com/LegalizeAdulthood/tgautils/actions/workflows/cmake.yml/badge.svg)](https://github.com/LegalizeAdulthood/tgautils/actions/workflows/cmake.yml)

# Obtaining the Source

Use git to clone this repository, then update the vcpkg submodule to bootstrap
the dependency process.

```
git clone https://github.com/LegalizeAdulthood/tgautils
cd tgautils
git submodule init
git submodule update --depth 1
```

# Building

A CMake preset has been provided to perform the usual CMake steps of
configure, build and test.

```
cmake --workflow --preset default
```

Places the build outputs in a sibling directory of the source code directory, e.g. up
and outside of the source directory.
