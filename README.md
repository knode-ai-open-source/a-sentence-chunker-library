# Overview of A Sentence Chunker Library


## Dependencies

* [A cmake library](https://github.com/knode-ai-open-source/a-cmake-library) needed for the cmake build
* [A memory library](https://github.com/knode-ai-open-source/a-memory-library) for the memory handling
* [The macro library](https://github.com/knode-ai-open-source/the-macro-library) for sorting, searching, and conversions
* [A JSON library](https://github.com/knode-ai-open-source/a-json-library) for parsing json


## Building

### Build and Install
```bash
mkdir -p build
cd build
cmake ..
make
sudo make install
```

### Uninstall
```bash

```

### Build Tests and Test Coverage
```bash
mkdir -p build
cd build
cmake BUILD_TESTING=ON ENABLE_CODE_COVERAGE=ON ..
make
ctest
make coverage
```
