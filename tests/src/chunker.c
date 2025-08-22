// SPDX-FileCopyrightText: 2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include "a-json-library/ajson.h"
#include "a-memory-library/aml_pool.h"
#include "a-sentence-chunker-library/a_sentence_chunker.h"

#define MAX_PATH_LEN 1024

// Helper function: read entire file into memory.
char *read_file(const char *filename, size_t *out_length) {
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("fopen");
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);
    char *buffer = malloc(fsize + 1);
    if (!buffer) {
        fclose(fp);
        return NULL;
    }
    fread(buffer, 1, fsize, fp);
    fclose(fp);
    buffer[fsize] = '\0';
    if (out_length)
        *out_length = fsize;
    return buffer;
}

static void print_with_escaped_newlines(const char *str) {
    while (*str) {
        if (*str == '\n') {
            printf("\\n");
        } else {
            putchar(*str);
        }
        str++;
    }
}


// ------------------------------------------------------------------
// Process a NON-json file: read its contents, chunk into sentences,
// and print each sentence on its own line.
// ------------------------------------------------------------------
static void process_non_json_file(const char *filename) {
    size_t length = 0;
    char *content = read_file(filename, &length);
    if (!content) {
        fprintf(stderr, "Could not read file: %s\n", filename);
        return;
    }
    // Create buffers for sentence chunking
    aml_buffer_t *bh1 = aml_buffer_init(32);
    aml_buffer_t *bh2 = aml_buffer_init(32);

    // First pass
    size_t num_first_chunks = 0;
    a_sentence_chunk_t *first_chunks = a_sentence_chunker(&num_first_chunks, bh1, content);

    // Second pass (enforces min/max length, etc.)
    size_t num_chunks = 0;
    a_sentence_chunk_t *chunks = a_rechunk_sentences(
        &num_chunks,
        bh2,
        content,
        first_chunks,
        num_first_chunks,
        5,    // min_length
        250   // max_length
    );

    // Print each sentence on its own line
    for (size_t i = 0; i < num_chunks; i++) {
        a_sentence_chunk_t *c = &chunks[i];
        size_t off = c->start_offset;
        size_t ln = c->length;
        // Safety check to avoid going past end
        if (off + ln > length) {
            ln = (off < length) ? (length - off) : 0;
        }

        char *sentence = malloc(ln + 1);
        memcpy(sentence, content + off, ln);
        sentence[ln] = '\0';

		print_with_escaped_newlines(sentence);
		putchar('\n');
        free(sentence);
    }

    aml_buffer_destroy(bh1);
    aml_buffer_destroy(bh2);
    free(content);
}

// ------------------------------------------------------------------
// Process a JSON file containing tests (unchanged).
// ------------------------------------------------------------------
static void process_json_file(const char *json_file) {
    size_t json_len = 0;
    char *json_content = read_file(json_file, &json_len);
    if (!json_content) {
        fprintf(stderr, "Could not read JSON file: %s\n", json_file);
        return;
    }

    // Create a memory pool for this file's tests.
    aml_pool_t *pool = aml_pool_init(1024 * 1024);
    ajson_t *root = ajson_parse_string(pool, json_content);
    free(json_content);
    if (ajson_is_error(root) || ajson_type(root) != object) {
        fprintf(stderr, "Invalid JSON in file: %s\n", json_file);
        aml_pool_destroy(pool);
        return;
    }

    // Get the "tests" array
    ajson_t *tests_array = ajsono_get(root, "tests");
    if (!tests_array || ajson_is_error(tests_array) || ajson_type(tests_array) != array) {
        fprintf(stderr, "No valid 'tests' array in file: %s\n", json_file);
        aml_pool_destroy(pool);
        return;
    }

    size_t test_count = ajsona_count(tests_array);
    size_t total_tests = test_count;
    size_t passed_tests = 0;

    printf("\n=== Processing JSON file: %s ===\n", json_file);

    for (size_t i = 0; i < test_count; i++) {
        ajson_t *test_obj = ajsona_scan(tests_array, (int)i);
        if (!test_obj || ajson_is_error(test_obj) || ajson_type(test_obj) != object) {
            fprintf(stderr, "Test %zu is not a valid object.\n", i);
            continue;
        }

        const char *source_text = ajsono_scan_strd(pool, test_obj, "source_text", "");
        if (!source_text || !*source_text) {
            fprintf(stderr, "Test %zu has no source_text.\n", i);
            continue;
        }

        // Get expected sentences
        ajson_t *expected_node = ajsono_get(test_obj, "expected");
        size_t expected_count = 0;
        char **expected_sentences = NULL;
        if (expected_node && !ajson_is_error(expected_node)) {
            if (ajson_type(expected_node) == array) {
                expected_count = ajsona_count(expected_node);
                expected_sentences = malloc(expected_count * sizeof(char *));
                for (size_t j = 0; j < expected_count; j++) {
                    ajson_t *valnode = ajsona_scan(expected_node, (int)j);
                    expected_sentences[j] = ajson_to_strd(pool, valnode, "");
                }
            } else if (ajson_type(expected_node) == string) {
                expected_count = 1;
                expected_sentences = malloc(sizeof(char *));
                expected_sentences[0] = ajson_to_strd(pool, expected_node, "");
            }
        } else {
            fprintf(stderr, "Test %zu has no valid expected field.\n", i);
            continue;
        }

        // Create AML buffers for sentence chunking.
        aml_buffer_t *bh1 = aml_buffer_init(32);
        aml_buffer_t *bh2 = aml_buffer_init(32);

        // First-pass sentence chunking
        size_t num_first_chunks = 0;
        a_sentence_chunk_t *first_chunks = a_sentence_chunker(&num_first_chunks, bh1, source_text);

        // Second-pass re-chunking (if needed)
        size_t num_chunks = 0;
        a_sentence_chunk_t *chunks = a_rechunk_sentences(
            &num_chunks,
            bh2,
            source_text,
            first_chunks,
            num_first_chunks,
            5,       // min_length
            200      // max_length
        );

        // =========================
        // Detailed comparison code
        // =========================

        int test_pass = 1;

        // Build a list of actual sentences from 'chunks'
        char **actual_sentences = NULL;
        if (num_chunks > 0) {
            actual_sentences = malloc(num_chunks * sizeof(char *));
            for (size_t j = 0; j < num_chunks; j++) {
                a_sentence_chunk_t *c = &chunks[j];
                size_t off = c->start_offset;
                size_t ln = c->length;
                size_t source_len = strlen(source_text);

                // Ensure we don't go out of bounds
                if (off + ln > source_len) {
                    ln = (off < source_len) ? (source_len - off) : 0;
                }
                char *s = malloc(ln + 1);
                memcpy(s, source_text + off, ln);
                s[ln] = '\0';
                actual_sentences[j] = s;
            }
        }

        // Compare up to the smaller of num_chunks and expected_count
        size_t common_count = (num_chunks < expected_count) ? num_chunks : expected_count;
        for (size_t j = 0; j < common_count; j++) {
            if (strcmp(actual_sentences[j], expected_sentences[j]) != 0) {
                printf("Test %zu, Sentence %zu: FAIL (mismatch)\n", i, j);
                printf("  Expected: [%s]\n", expected_sentences[j]);
                printf("  Got:      [%s]\n", actual_sentences[j]);
                test_pass = 0;
            }
        }

        // If there are fewer actual chunks than expected, show the missing ones
        if (num_chunks < expected_count) {
            size_t missing_count = expected_count - num_chunks;
            printf("Test %zu: Missing %zu sentences:\n", i, missing_count);
            for (size_t j = num_chunks; j < expected_count; j++) {
                printf("  (Missing) Expected sentence %zu: [%s]\n", j, expected_sentences[j]);
            }
            test_pass = 0;
        }

        // If there are extra actual chunks beyond what was expected, show the extras
        if (num_chunks > expected_count) {
            size_t extra_count = num_chunks - expected_count;
            printf("Test %zu: Extra %zu sentences:\n", i, extra_count);
            for (size_t j = expected_count; j < num_chunks; j++) {
                printf("  (Extra) Got sentence %zu: [%s]\n", j, actual_sentences[j]);
            }
            test_pass = 0;
        }

        // Final pass/fail for this test
        if (test_pass) {
            printf("Test %zu: PASS\n", i);
            passed_tests++;
        } else {
            printf("Test %zu: FAILED\n", i);
        }

        // Cleanup
        if (actual_sentences) {
            for (size_t j = 0; j < num_chunks; j++) {
                free(actual_sentences[j]);
            }
            free(actual_sentences);
        }
        aml_buffer_destroy(bh1);
        aml_buffer_destroy(bh2);
        free(expected_sentences);
    }

    printf("\nSummary for file %s: %zu/%zu tests passed.\n", json_file, passed_tests, total_tests);
    aml_pool_destroy(pool);
}

// Recursively process all JSON files in a directory.
static void process_directory(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("opendir");
        return;
    }
    struct dirent *entry;
    char path[MAX_PATH_LEN];

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        struct stat path_stat;
        if (stat(path, &path_stat) != 0) {
            perror("stat");
            continue;
        }
        if (S_ISDIR(path_stat.st_mode)) {
            process_directory(path);
        } else if (S_ISREG(path_stat.st_mode) && strstr(entry->d_name, ".json")) {
            // It's a JSON file -> process as test JSON
            printf("\nProcessing JSON file: %s\n", path);
            process_json_file(path);
        }
    }
    closedir(dir);
}

// ------------------------------------------------------------------
// main: decides how to handle the single path argument.
// ------------------------------------------------------------------
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <test.json | directory>\n", argv[0]);
        return 1;
    }

    struct stat path_stat;
    if (stat(argv[1], &path_stat) != 0) {
        perror("stat");
        return 1;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        // It's a directory -> recursively handle .json files
        process_directory(argv[1]);
    }
    else if (S_ISREG(path_stat.st_mode)) {
        // It's a regular file -> check if extension is .json
        const char *filename = argv[1];
        const char *dot = strrchr(filename, '.');
        if (dot && strcmp(dot, ".json") == 0) {
            // If it's .json -> process as JSON test file
            process_json_file(filename);
        } else {
            // Otherwise, chunk it and print one sentence per line
            process_non_json_file(filename);
        }
    }
    else {
        fprintf(stderr, "Unsupported file type.\n");
    }

    return 0;
}
