# SPDX-FileCopyrightText: 2024-2025 Knode.ai
# SPDX-License-Identifier: Apache-2.0
# Maintainer: Andy Curtis <contactandyc@gmail.com>
cmake_minimum_required(VERSION 3.10)
project(a_sentence_chunker_library_example VERSION 1.0)

# Enable testing
enable_testing()

# Set the directory for test sources
set(TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/chunker.c)

set(CUSTOM_PACKAGES a-sentence-chunker-library a-json-library a-memory-library the-macro-library the-lz4-library the-io-library)
set(THIRD_PARTY_PACKAGES ZLIB)

find_package(a-cmake-library REQUIRED)

include(BinaryConfig)
