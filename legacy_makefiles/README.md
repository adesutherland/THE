# Legacy Makefiles

This directory contains the original Makefiles and build scripts that were used before the project migrated to CMake. They have been preserved here for reference purposes, in case you need to consult the specific compiler flags, definitions, or custom build steps used on older platforms (e.g., OS/2, Amiga, DOS, older Windows).

To build the project now, please use the standard CMake workflow from the project root:

```bash
mkdir -p cmake-build
cd cmake-build
cmake ..
make
```

For CLion users, the project should now load properly via the root `CMakeLists.txt`.
