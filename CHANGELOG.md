## 08/22/2025

**Refactor build & packaging for A Sentence Chunker Library**

## Summary:

This PR restructures the build system, modernizes packaging, improves metadata, and introduces testing/coverage tooling for **A Sentence Chunker Library**.

### Build System

* Removed **Changie** configuration and generated files (`.changie.yaml`, `.changes/*`, `CHANGELOG.md`, `NEWS.md`, `build_install.sh`).
* Added **unified `build.sh`** with commands: `build`, `install`, `coverage`, `clean`.
* Updated `.gitignore` to include additional build directories.
* Replaced legacy CMake config with **variant-based CMakeLists**:

    * Supports `debug`, `memory`, `static`, `shared` builds.
    * Provides umbrella alias `a_sentence_chunker_library::a_sentence_chunker_library`.
    * Generates proper `Config.cmake` + `Targets.cmake` for downstream projects.
* Added **coverage instrumentation** options (Clang/GCC).
* Tests integrated under `tests/` with `ctest`.

### Docker

* Replaced dependency on `dev-env` with a standalone **Ubuntu-based Dockerfile**:

    * Installs CMake directly from Kitware releases.
    * Adds optional dev tools (valgrind, gdb, Python, autotools).
    * Builds required deps: `a-memory-library`, `the-lz4-library`, `the-macro-library`, `the-io-library`.
    * Compiles and installs this project into `/usr/local`.

### Metadata

* Updated **AUTHORS** to include GitHub profile.
* Updated **NOTICE** with clear copyright attribution:

    * `© 2025 Andy Curtis`
    * `© 2024–2025 Knode.ai`
* Refreshed SPDX headers across all source and header files with consistent attribution and technical contact.

### Tests

* Reworked `tests/CMakeLists.txt`:

    * Uses modern CMake (≥3.20).
    * Adds unified `coverage_report` target with `llvm-profdata` + `llvm-cov` HTML output.
* Added `tests/build.sh` for simplified test builds and coverage runs.
* Updated test sources with SPDX metadata.

### Outcome

This refactor:

* Standardizes build/packaging with modern CMake and Docker practices.
* Provides explicit dependency management.
* Introduces coverage tooling for better QA.
* Cleans up stale changelog/release management files.
