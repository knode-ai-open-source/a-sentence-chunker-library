# a-sentence-chunker-library

Light‑weight C utility to segment raw UTF-8 (or ASCII) text into sentence “chunks”, with an optional **re‑chunking pass** to enforce min/max sentence sizes. Pairs naturally with **a-memory-library** buffers (`aml_buffer_t`) for zero‑copy substring access.

## Status

> Early version derived from the public header (API may evolve). Contributions and issues welcome.

## Why?

Many NLP / embedding / LLM pipelines need: (1) sentence boundaries and (2) roughly uniform chunk lengths (tokens or chars). A single greedy split often yields very short or very long sentences. This library provides a *two‑phase* approach:

1. **First pass** – fast heuristic sentence boundary detection (punctuation, newline handling, etc.).
2. **Second pass** – optional `a_rechunk_sentences` merges adjacent short sentences and/or splits overly long ones to stay within `[min_length, max_length]` (character counts) while preserving original offsets.

## Core Types

```c
typedef struct {
    size_t start_offset; // byte offset in original text
    size_t length;       // byte length of the sentence
} a_sentence_chunk_t;
```

You work with *views* (offset+length) instead of allocating substring copies.

## API Overview

```c
a_sentence_chunk_t *a_sentence_chunker(
    size_t *num,
    aml_buffer_t *buffer,
    const char *text);

a_sentence_chunk_t *a_rechunk_sentences(
    size_t *num,
    aml_buffer_t *second_buffer,
    const char *text,
    a_sentence_chunk_t *first_pass_chunks,
    size_t first_pass_count,
    size_t min_length,
    size_t max_length);
```

### Parameters (both functions)

* `num` (out): receives count of chunks produced.
* `*buffer` / `*second_buffer`: target `aml_buffer_t` where the array of `a_sentence_chunk_t` is (re)allocated/expanded.
* `text`: NUL‑terminated source text (not copied).

### First Pass

Returns an array of `a_sentence_chunk_t` describing raw sentence candidates.

### Re-chunk Pass

Consumes first-pass result; returns a *new* (or resized) array of refined chunks obeying length constraints. Typical usage: set `min_length` to avoid ultra-short sentences (e.g. 40–60 chars) and `max_length` to cap very long ones (e.g. 500–800 chars). Splitting strategy is implementation‑defined (likely at whitespace boundaries) – consult source for exact heuristics.

## Example

```c
#include "a-sentence-chunker-library/a_sentence_chunker.h"
#include "a-memory-library/aml_buffer.h"

void process(const char *text) {
    aml_buffer_t buf1 = {0};
    size_t n1 = 0;
    a_sentence_chunk_t *chunks = a_sentence_chunker(&n1, &buf1, text);

    // Optional second pass: enforce 60–400 char bounds
    aml_buffer_t buf2 = {0};
    size_t n2 = 0;
    a_sentence_chunk_t *final_chunks = a_rechunk_sentences(
        &n2, &buf2, text, chunks, n1, 60, 400);

    for(size_t i=0;i<n2;i++) {
        const a_sentence_chunk_t *c = &final_chunks[i];
        fwrite(text + c->start_offset, 1, c->length, stdout);
        fputs("
---
", stdout);
    }

    // Free buffers per a-memory-library's lifecycle utilities (not shown).
}
```

## Memory & Ownership

* Returned pointer lives inside the provided `aml_buffer_t`; you do **not** `free()` it directly.
* The original `text` must remain valid while using chunk offsets.
* Re-chunking uses a *separate* buffer so you can keep / compare first-pass results if desired.

## Thread Safety

No global state; each invocation is independent. Provide distinct buffers per thread.

## Complexity

O(n) over bytes for first pass. Re-chunking is O(k) over produced sentences (k ≤ n). Memory: O(k) \* sizeof(chunk).

## Integration Tips

* Token lengths: If you need token counts, you can later map char spans to tokens (e.g. using a BPE tokenizer) because you have precise offsets.
* Streaming: Accumulate input into a buffer, then run the chunker once complete (no incremental API yet).

## Error Handling

Functions likely return `NULL` (with `*num == 0`) on allocation failure; check results. (Adjust once the implementation defines explicit error codes.)

## Testing Ideas

* Punctuation edge cases: "e.g.", "Dr.", ellipses, quotes.
* Mixed newline formats (CRLF vs LF).
* Very long paragraphs (verify splitting at `max_length`).
* Unicode multi-byte characters (ensure offsets count bytes, not code points).

## License

Apache-2.0 (see SPDX tags in headers).

## Roadmap (Potential)

* Configurable abbreviation list.
* Token-count based rechunking.
* Incremental / streaming boundary detection.
* Optional JSON / debug dump helper.
