// SPDX-FileCopyrightText: 2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include <ctype.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "a-sentence-chunker-library/a_sentence_chunker.h"

// ----------------------------------------------------------------------------
//                          HELPER FUNCTIONS
// ----------------------------------------------------------------------------

static inline bool is_sentence_punct(char c) {
    return (c == '.' || c == '?' || c == '!');
}

/* Some known abbreviations to skip. Expand as desired. */
static const char * ABBREVS[] = {
    "Mr",       // Mister
    "Mrs",      // Mistress
    "Ms",       // (Generic title)
    "Dr",       // Doctor
    "St",       // Street or Saint
    "etc",      // Etcetera
    "i.e",      // id est
    "e.g",      // exempli gratia
    "vs",       // versus
    "Inc",      // Incorporated
    "Corp",     // Corporation
    "Ltd",      // Limited
    "Co",       // Company
    "Jr",       // Junior
    "Sr",       // Senior
    "Ph.D",     // Doctor of Philosophy
    NULL
};

static bool is_whitespace(char c) {
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

/*
   skip_spaces: Return the index of the next non-whitespace character,
   or 'len' if none found.
*/
static size_t skip_spaces(const char *text, size_t start, size_t len) {
    size_t j = start;
    while (j < len && is_whitespace(text[j])) {
        j++;
    }
    return j;
}

static inline bool is_alpha(char c) {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}

/*
   Move backward until whitespace or start-of-string or '.' to isolate
   the preceding word. Then see if it matches known abbreviations.
*/
static bool matches_abbreviation(const char *text, size_t i) {
    if (i == 0) return false; // no room
    // i points at '.'
    int start = (int)i - 1;
    while (start >= 0 && !is_whitespace(text[start])) {
        start--;
    }
    int abbrev_len = (int)i - (start + 1);
    if (abbrev_len <= 0) return false;

    // If next character is alpha, treat '.' as an abbreviation boundary
    if (is_alpha(text[i+1])) {
        return true;
    }

    // If exactly one uppercase letter, treat as abbreviation.
    if (abbrev_len == 1 && isupper((unsigned char)text[start+1])) {
        return true;
    }

    // Single letter abbreviation followed by non-whitespace
    if (abbrev_len == 1 && !is_whitespace(text[i+1])) {
        return true;
    }

    // Copy preceding word to a small buffer
    char buf[32];
    if (abbrev_len >= (int)sizeof(buf)) {
        return false; // too large
    }
    memcpy(buf, &text[start+1], abbrev_len);
    buf[abbrev_len] = '\0';

    // Compare to known abbreviations (case-insensitive)
    for (int idx = 0; ABBREVS[idx] != NULL; idx++) {
        if (strcasecmp(buf, ABBREVS[idx]) == 0) {
            return true;
        }
    }
    return false;
}

/*
   Return true if text[start..i-1] is purely digits.
*/
static bool is_just_digits(const char *text, size_t start, size_t i) {
    if (i <= start) return false;
    for (size_t pos = start; pos < i; pos++) {
        if (!isdigit((unsigned char)text[pos])) {
            return false;
        }
    }
    return true;
}

/*
   is_end_of_sentence_heuristic:
   Decide if punctuation at index i is an end-of-sentence boundary,
   or if we should skip it for e.g. decimals, abbreviations, etc.
*/
static bool is_end_of_sentence_heuristic(const char *text, size_t i, size_t len) {
    char c = text[i];

    // 1) Skip decimals: If '.' is between two digits => "3.14"
    if (c == '.' && i > 0 && i < len - 1) {
        if (isdigit((unsigned char)text[i-1]) && isdigit((unsigned char)text[i+1])) {
            return false;
        }
    }

    // 2) Skip known abbreviations: "Mr.", "Dr."
    if (c == '.') {
        if (matches_abbreviation(text, i)) {
            return false;
        }
    }

    // 3) Ordinal lists: "1.", "2."
    //    If substring before '.' is digits only, and
    //    next non-whitespace is digit or lowercase => skip
    if (c == '.') {
        // find start of word
        size_t word_start = i;
        while (word_start > 0 &&
               !is_whitespace(text[word_start - 1]) &&
               text[word_start - 1] != '.')
        {
            word_start--;
        }
        if (is_just_digits(text, word_start, i)) {
            size_t j = skip_spaces(text, i + 1, len);
            if (j >= len) {
                // end of text => not a real separate sentence
                return false;
            }
            if (isdigit((unsigned char)text[j]) ||
                islower((unsigned char)text[j]))
            {
                // e.g. "1. 2" or "1. next"
                return false;
            }
        }
    }

    // If we get here, treat '.' or '?' or '!' as a real boundary.
    return true;
}

/*
   consume_multiple_punctuation:
   If we encounter consecutive punctuation like "!!" or "...", treat them as one run.
   Return the index of the last punctuation in that consecutive run.
*/
static size_t consume_multiple_punctuation(const char *text,
                                           size_t start_i,
                                           size_t len)
{
    size_t i = start_i;
    while ((i + 1) < len && is_sentence_punct(text[i + 1])) {
        i++;
    }
    return i;
}

/*
   consume_trailing_closers:
   Include trailing quotes/brackets after the end punctuation.
   E.g. "Hello (anyone?)."
*/
static size_t consume_trailing_closers(const char *text, size_t i, size_t len)
{
    while ((i + 1) < len) {
        char next_char = text[i + 1];
        if (next_char == '\"' || next_char == '\'' ||
            next_char == ')'  || next_char == ']'  ||
            next_char == '}'  || is_sentence_punct(next_char))
        {
            i++;
        }
        else {
            break;
        }
    }
    return i;
}

// ----------------------------------------------------------------------------
//                     FIRST PASS: CHUNK INTO SENTENCES
// ----------------------------------------------------------------------------

a_sentence_chunk_t *a_sentence_chunker(
    size_t *num_sentences_out,
    aml_buffer_t *bh,
    const char *text)
{
    aml_buffer_clear(bh);
    *num_sentences_out = 0;
    if (!text || !*text) {
        return NULL;
    }

    size_t len = strlen(text);
    size_t start_off = 0;
    size_t i = 0;

    while (i < len) {
        char c = text[i];

        if (is_sentence_punct(c)) {
            // Gather consecutive punctuation
            size_t last_punct = consume_multiple_punctuation(text, i, len);

            // Check if it's end-of-sentence
            if (is_end_of_sentence_heuristic(text, last_punct, len)) {
                // Include any trailing closers
                last_punct = consume_trailing_closers(text, last_punct, len);

                // Boundary is [start_off.. last_punct+1]
                size_t boundary_len = (last_punct + 1) - start_off;
                if (boundary_len > 0) {
                    a_sentence_chunk_t sb;
                    sb.start_offset = start_off;
                    sb.length = boundary_len;
                    aml_buffer_append(bh, &sb, sizeof(sb));
                }

                // Next sentence starts after last_punct + 1
                i = last_punct + 1;
                start_off = i;

                // Skip trailing spaces
                while (start_off < len && is_whitespace(text[start_off])) {
                    start_off++;
                }
                continue;
            }
            else {
                // Not a boundary -> skip punctuation
                i = last_punct + 1;
                continue;
            }
        }
        else {
            // Normal character
            i++;
        }
    }

    // Capture leftover from [start_off..end]
    if (start_off < len) {
        size_t boundary_len = len - start_off;
        if (boundary_len > 0) {
            a_sentence_chunk_t sb;
            sb.start_offset = start_off;
            sb.length = boundary_len;
            aml_buffer_append(bh, &sb, sizeof(sb));
        }
    }

    // Build array
    size_t total = aml_buffer_length(bh) / sizeof(a_sentence_chunk_t);
    if (total == 0) {
        return NULL;
    }
    a_sentence_chunk_t *array = (a_sentence_chunk_t *)aml_buffer_data(bh);
    *num_sentences_out = total;
    return array;
}

// ----------------------------------------------------------------------------
//        SECOND PASS: LENGTH-BASED RE-CHUNKING WITHOUT SPLITTING TOKENS
// ----------------------------------------------------------------------------

/*
   Adjust the candidate split index so that we never split in the
   middle of a token (defined here as a contiguous run of non-whitespace).
   - We prefer to move backward to the nearest whitespace, if any is found
     between [start_offset+1.. i].
   - Otherwise, we move forward to the next whitespace if possible.
   - If no whitespace can be found in either direction, we skip splitting.
*/
static size_t adjust_for_token_boundary(const char *text,
                                        size_t chunk_start,
                                        size_t chunk_end,
                                        size_t candidate)
{
    // Safety checks
    if (candidate <= chunk_start || candidate >= chunk_end) {
        return candidate; // out-of-range or edge => no change
    }

    // 1) Try to move backward until we hit whitespace
    {
        size_t j = candidate;
        while (j > chunk_start) {
            if (is_whitespace(text[j])) {
                return j; // found a suitable boundary
            }
            j--;
        }
    }
    // 2) If that fails, try moving forward until we hit whitespace
    {
        size_t j = candidate;
        while (j < chunk_end) {
            if (is_whitespace(text[j])) {
                return j; // found a suitable boundary
            }
            j++;
        }
    }
    // 3) If no whitespace found in the entire chunk, we cannot split
    return 0; // signal "no valid boundary" => skip
}

/*
   find_split_point: tries to find a suitable break point within [start_offset..(start_offset+length)]
   that satisfies min_length <= chunk <= max_length and doesn't break tokens.
*/
size_t find_split_point(const char *text, size_t start_offset, size_t length,
                        size_t min_length, size_t max_length)
{
    size_t end_offset = start_offset + length;

    // If within max_length, no need to split
    if (length <= max_length) {
        return end_offset;
    }

    // We'll look for a possible break between [search_start..search_end].
    size_t search_start = start_offset + min_length;
    size_t valid_split_end = end_offset - min_length;
    size_t search_end = start_offset + max_length;

    // If the "ideal" search_end is beyond valid_split_end, no beneficial split
    if (search_end > valid_split_end) {
        return end_offset;
    }
    // If the window is invalid or too narrow
    if (search_start >= search_end) {
        return end_offset;
    }

    // ============== EXISITING HEURISTICS ==============

    // Heuristic 1: 2 consecutive newlines
    for (size_t i = search_end; i > search_start; i--) {
        if ((i - 1) >= search_start && i < end_offset &&
            text[i - 1] == '\n' && text[i] == '\n')
        {
            // Adjust for token boundary
            size_t adjusted = adjust_for_token_boundary(text, start_offset, end_offset, i);
            if (adjusted > start_offset && adjusted < end_offset) {
                return adjusted;
            }
            else {
                return end_offset; // skip if no valid boundary
            }
        }
    }

    // Heuristic 1b: 3 whitespace chars in a row
    for (size_t i = search_end; i > search_start; i--) {
        if ((i - 2) >= search_start && i < end_offset &&
            isspace((unsigned char)text[i - 2]) &&
            isspace((unsigned char)text[i - 1]) &&
            isspace((unsigned char)text[i]))
        {
            size_t adjusted = adjust_for_token_boundary(text, start_offset, end_offset, i);
            if (adjusted > start_offset && adjusted < end_offset) {
                return adjusted;
            }
            else {
                return end_offset;
            }
        }
    }

    // Heuristic 2: single newline
    for (size_t i = search_end; i > search_start; i--) {
        if (text[i] == '\n') {
            size_t adjusted = adjust_for_token_boundary(text, start_offset, end_offset, i);
            if (adjusted > start_offset && adjusted < end_offset) {
                return adjusted;
            }
            else {
                return end_offset;
            }
        }
    }

    // Heuristic 3: punctuation + whitespace + uppercase letter
    for (size_t i = search_end; i > search_start; i--) {
        if (i < end_offset) {
            char prev = text[i - 1];
            char curr = text[i];
            if ((prev == '.' || prev == '!' || prev == '?') && is_whitespace((unsigned char)curr)) {
                // Check if next non-whitespace is uppercase
                size_t j = i + 1;
                while (j < end_offset && is_whitespace((unsigned char)text[j])) {
                    j++;
                }
                if (j < end_offset && isupper((unsigned char)text[j])) {
                    size_t adjusted = adjust_for_token_boundary(text, start_offset, end_offset, i);
                    if (adjusted > start_offset && adjusted < end_offset) {
                        return adjusted;
                    }
                    else {
                        return end_offset;
                    }
                }
            }
        }
    }

    // Heuristic 4: fallback - any whitespace in the allowed range
    for (size_t i = search_end; i > search_start; i--) {
        if (isspace((unsigned char)text[i])) {
            size_t adjusted = adjust_for_token_boundary(text, start_offset, end_offset, i);
            if (adjusted > start_offset && adjusted < end_offset) {
                return adjusted;
            }
            else {
                return end_offset;
            }
        }
    }

    // ============== NO HEURISTIC FOUND ==============
    // Fall back to search_end -> but must adjust for token boundary
    {
        size_t adjusted = adjust_for_token_boundary(text, start_offset, end_offset, search_end);
        if (adjusted > start_offset && adjusted < end_offset) {
            return adjusted;
        }
        else {
            return end_offset; // skip
        }
    }
}

/*
   a_rechunk_sentences: Takes the first pass of chunked sentences
   and merges/splits them based on min_length/max_length, but ensures
   we never split in the middle of a token.
*/
a_sentence_chunk_t *a_rechunk_sentences(
    size_t *num_sentences_out,
    aml_buffer_t *second_buffer,
    const char *text,
    a_sentence_chunk_t *first_pass_chunks,
    size_t first_pass_count,
    size_t min_length,
    size_t max_length)
{
    aml_buffer_clear(second_buffer);
    *num_sentences_out = 0;

    for (size_t i = 0; i < first_pass_count; i++) {
        a_sentence_chunk_t current = first_pass_chunks[i];
        size_t chunk_start = current.start_offset;
        size_t chunk_length = current.length;

        // CASE 1: length within [min_length, max_length]
        if (chunk_length >= min_length && chunk_length <= max_length) {
            aml_buffer_append(second_buffer, &current, sizeof(current));
            continue;
        }
        // CASE 2: chunk is too short => try merging with previous or next
        else if (chunk_length < min_length) {
            bool merged = false;
            // Attempt to merge with the previously appended chunk if that won't exceed max_length
            if (i > 0) {
                // Access the last chunk in second_buffer
                a_sentence_chunk_t *last =
                    (a_sentence_chunk_t *)aml_buffer_end(second_buffer) - 1;
                // New combined length
                size_t combined_len = (current.start_offset + current.length)
                                    - last->start_offset;
                if (combined_len <= max_length) {
                    last->length = combined_len;
                    merged = true;
                }
            }

            // If not merged with the previous chunk, try merging forward with the next chunk
            if (!merged && (i + 1) < first_pass_count) {
                size_t next_start = first_pass_chunks[i + 1].start_offset;
                size_t next_len   = first_pass_chunks[i + 1].length;
                size_t combined_len = (next_start + next_len) - current.start_offset;
                if (combined_len <= max_length) {
                    // Merge them: we skip appending 'current' alone,
                    // and create a new merged chunk that covers both.
                    a_sentence_chunk_t merged_chunk;
                    merged_chunk.start_offset = current.start_offset;
                    merged_chunk.length = combined_len;
                    aml_buffer_append(second_buffer, &merged_chunk, sizeof(merged_chunk));
                    i++;  // skip the next chunk because it's merged
                    continue;
                }
            }

            // If we never merged, just append as is
            if (!merged) {
                aml_buffer_append(second_buffer, &current, sizeof(current));
            }
        }
        // CASE 3: chunk is too long => split
        else {
            a_sentence_chunk_t remaining = current;
            while (remaining.length > max_length) {
                size_t split_pt = find_split_point(
                    text,
                    remaining.start_offset,
                    remaining.length,
                    min_length,
                    max_length
                );
                // If no valid split found or split == entire chunk, we give up
                if (split_pt <= remaining.start_offset ||
                    split_pt >= (remaining.start_offset + remaining.length))
                {
                    // just break and append the leftover whole
                    break;
                }

                // Create the sub-chunk
                a_sentence_chunk_t chunk;
                chunk.start_offset = remaining.start_offset;
                chunk.length = split_pt - remaining.start_offset;
                aml_buffer_append(second_buffer, &chunk, sizeof(chunk));

                // Update "remaining" to reflect leftover
                remaining.length =
                    (remaining.start_offset + remaining.length) - split_pt;
                remaining.start_offset = split_pt;
            }
            // Append leftover
            aml_buffer_append(second_buffer, &remaining, sizeof(remaining));
        }
    }

    // Build final array
    size_t total = aml_buffer_length(second_buffer) / sizeof(a_sentence_chunk_t);
    if (total == 0) {
        return NULL;
    }
    a_sentence_chunk_t *array = (a_sentence_chunk_t *)aml_buffer_data(second_buffer);
    *num_sentences_out = total;
    return array;
}
