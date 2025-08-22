// SPDX-FileCopyrightText: 2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#ifndef _a_sentence_chunker_h
#define _a_sentence_chunker_h

#include "a-memory-library/aml_buffer.h"
#include <stdio.h>

typedef struct {
    size_t start_offset; // Where the sentence begins in the original text
    size_t length;       // How many characters in this sentence
} a_sentence_chunk_t;

a_sentence_chunk_t *a_sentence_chunker(
	size_t *num,
    aml_buffer_t *bh,
    const char *text);

a_sentence_chunk_t *a_rechunk_sentences(
    size_t *num,
    aml_buffer_t *second_buffer,
    const char *text,
    a_sentence_chunk_t *first_pass_chunks,
    size_t first_pass_count,
    size_t min_length,
    size_t max_length);

#endif
