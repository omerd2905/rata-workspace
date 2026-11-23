#ifndef RATA_ENGINES_PROJECT_FILE_H
#define RATA_ENGINES_PROJECT_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/arena.h"
#include "core/status.h"

#define RATA_PROJECT_FORMAT_VERSION 1u
#define RATA_PROJECT_MAX_ENTRIES    256u
#define RATA_PROJECT_MAX_LINE       512u
#define RATA_PROJECT_KEY_MAX        48u
#define RATA_PROJECT_SECTION_MAX    48u
#define RATA_PROJECT_VALUE_MAX      256u

typedef struct RataProjectEntry {
    char section[RATA_PROJECT_SECTION_MAX];
    char key[RATA_PROJECT_KEY_MAX];
    char value[RATA_PROJECT_VALUE_MAX];
} RataProjectEntry;

/* Parsed or writable line-based project document.
 * `entries` memory is owned by the arena or caller buffer provided at init/parse. */
typedef struct RataProjectDoc {
    uint32_t version;
    RataProjectEntry *entries;
    size_t count;
    size_t cap;
} RataProjectDoc;

/* Initializes an empty writable project document backed by `arena` (owned by `arena`). */
RataStatus project_doc_init(RataProjectDoc *doc, size_t max_entries, RataArena *arena);

/* Appends or updates a (section, key, value) entry. Rejects invalid chars/newlines. */
RataStatus project_doc_set(RataProjectDoc *doc,
                           const char *section,
                           const char *key,
                           const char *value);

/* Looks up a key within a section. Returns pointer inside `doc` (owned by `doc`) or NULL. */
const char *project_doc_get(const RataProjectDoc *doc, const char *section, const char *key);

/* Convenience typed getters returning false if missing or malformed. */
bool project_doc_get_double(const RataProjectDoc *doc,
                            const char *section,
                            const char *key,
                            double *out);
bool project_doc_get_i32(const RataProjectDoc *doc,
                         const char *section,
                         const char *key,
                         int32_t *out);

/* Parses versioned text buffer into `doc` (entries allocated in `arena`, owned by `arena`).
 * Safely rejects malformed headers, unsupported versions, control chars, or syntax errors. */
RataStatus project_file_parse(const char *text,
                              size_t len,
                              RataProjectDoc *doc_out,
                              RataArena *arena);

/* Serializes `doc` into caller-owned `buf` of capacity `buf_cap`.
 * Writes NUL-terminated text and stores byte count (excluding NUL) in `written_out`. */
RataStatus project_file_serialize(const RataProjectDoc *doc,
                                  char *buf,
                                  size_t buf_cap,
                                  size_t *written_out);

/* File I/O helpers using standard C11 stdio. */
RataStatus project_file_save_path(const RataProjectDoc *doc, const char *path);
RataStatus project_file_load_path(const char *path, RataProjectDoc *doc_out, RataArena *arena);

#endif /* RATA_ENGINES_PROJECT_FILE_H */
