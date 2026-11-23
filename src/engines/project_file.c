#include "engines/project_file.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool is_valid_ident_str(const char *s, size_t max_len) {
    if (s == NULL || s[0] == '\0') {
        return false;
    }
    size_t len = 0u;
    while (s[len] != '\0') {
        unsigned char c = (unsigned char)s[len];
        if (!(isalnum(c) || c == '_' || c == '-' || c == '.')) {
            return false;
        }
        len++;
        if (len >= max_len) {
            return false;
        }
    }
    return true;
}

static bool is_valid_value_str(const char *s, size_t max_len) {
    if (s == NULL) {
        return false;
    }
    size_t len = 0u;
    while (s[len] != '\0') {
        unsigned char c = (unsigned char)s[len];
        if (c < 0x20u || c == 0x7Fu) {
            return false;
        }
        len++;
        if (len >= max_len) {
            return false;
        }
    }
    return true;
}

RataStatus project_doc_init(RataProjectDoc *doc, size_t max_entries, RataArena *arena) {
    if (doc == NULL || arena == NULL || max_entries == 0u ||
        max_entries > RATA_PROJECT_MAX_ENTRIES) {
        return RATA_ERR_INVALID_ARG;
    }
    RataProjectEntry *arr = (RataProjectEntry *)rata_arena_calloc(
        arena, max_entries, sizeof(RataProjectEntry), 8u);
    if (arr == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }
    doc->version = RATA_PROJECT_FORMAT_VERSION;
    doc->entries = arr;
    doc->count = 0u;
    doc->cap = max_entries;
    return RATA_OK;
}

RataStatus project_doc_set(RataProjectDoc *doc,
                           const char *section,
                           const char *key,
                           const char *value) {
    if (doc == NULL || doc->entries == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    const char *sec = (section == NULL) ? "" : section;
    if (sec[0] != '\0' && !is_valid_ident_str(sec, RATA_PROJECT_SECTION_MAX)) {
        return RATA_ERR_INVALID_ARG;
    }
    if (!is_valid_ident_str(key, RATA_PROJECT_KEY_MAX)) {
        return RATA_ERR_INVALID_ARG;
    }
    if (!is_valid_value_str(value, RATA_PROJECT_VALUE_MAX)) {
        return RATA_ERR_INVALID_ARG;
    }

    for (size_t i = 0u; i < doc->count; ++i) {
        if (strcmp(doc->entries[i].section, sec) == 0 &&
            strcmp(doc->entries[i].key, key) == 0) {
            snprintf(doc->entries[i].value, RATA_PROJECT_VALUE_MAX, "%s", value);
            return RATA_OK;
        }
    }
    if (doc->count >= doc->cap) {
        return RATA_ERR_OUT_OF_MEMORY;
    }
    RataProjectEntry *e = &doc->entries[doc->count++];
    snprintf(e->section, RATA_PROJECT_SECTION_MAX, "%s", sec);
    snprintf(e->key, RATA_PROJECT_KEY_MAX, "%s", key);
    snprintf(e->value, RATA_PROJECT_VALUE_MAX, "%s", value);
    return RATA_OK;
}

const char *project_doc_get(const RataProjectDoc *doc, const char *section, const char *key) {
    if (doc == NULL || doc->entries == NULL || key == NULL) {
        return NULL;
    }
    const char *sec = (section == NULL) ? "" : section;
    for (size_t i = 0u; i < doc->count; ++i) {
        if (strcmp(doc->entries[i].section, sec) == 0 &&
            strcmp(doc->entries[i].key, key) == 0) {
            return doc->entries[i].value;
        }
    }
    return NULL;
}

bool project_doc_get_double(const RataProjectDoc *doc,
                            const char *section,
                            const char *key,
                            double *out) {
    const char *v = project_doc_get(doc, section, key);
    if (v == NULL || v[0] == '\0' || out == NULL) {
        return false;
    }
    char *end = NULL;
    double val = strtod(v, &end);
    if (end == v || *end != '\0' || !isfinite(val)) {
        return false;
    }
    *out = val;
    return true;
}

bool project_doc_get_i32(const RataProjectDoc *doc,
                         const char *section,
                         const char *key,
                         int32_t *out) {
    const char *v = project_doc_get(doc, section, key);
    if (v == NULL || v[0] == '\0' || out == NULL) {
        return false;
    }
    char *end = NULL;
    long val = strtol(v, &end, 10);
    if (end == v || *end != '\0' || val < -2147483647L - 1L || val > 2147483647L) {
        return false;
    }
    *out = (int32_t)val;
    return true;
}

RataStatus project_file_parse(const char *text,
                              size_t len,
                              RataProjectDoc *doc_out,
                              RataArena *arena) {
    if (doc_out != NULL) {
        memset(doc_out, 0, sizeof(*doc_out));
    }
    if (text == NULL || doc_out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (len == 0u || len > 1024u * 1024u) {
        return RATA_ERR_FORMAT;
    }

    size_t mark = rata_arena_mark(arena);
    size_t rem_bytes = rata_arena_remaining(arena);
    size_t max_fit = (rem_bytes > 16u) ? ((rem_bytes - 16u) / sizeof(RataProjectEntry)) : 0u;
    size_t cap = (max_fit < RATA_PROJECT_MAX_ENTRIES) ? max_fit : RATA_PROJECT_MAX_ENTRIES;
    if (cap == 0u) {
        return RATA_ERR_OUT_OF_MEMORY;
    }
    RataStatus st = project_doc_init(doc_out, cap, arena);
    if (st != RATA_OK) {
        return st;
    }

    bool saw_header = false;
    char cur_section[RATA_PROJECT_SECTION_MAX];
    cur_section[0] = '\0';

    size_t pos = 0u;
    while (pos < len) {
        size_t line_start = pos;
        while (pos < len && text[pos] != '\n') {
            if (text[pos] == '\0') {
                rata_arena_restore(arena, mark);
                return RATA_ERR_FORMAT;
            }
            pos++;
        }
        size_t line_end = pos;
        if (pos < len && text[pos] == '\n') {
            pos++;
        }
        if (line_end > line_start && text[line_end - 1u] == '\r') {
            line_end--;
        }

        /* Trim leading/trailing spaces on line. */
        while (line_start < line_end &&
               (text[line_start] == ' ' || text[line_start] == '\t')) {
            line_start++;
        }
        while (line_end > line_start &&
               (text[line_end - 1u] == ' ' || text[line_end - 1u] == '\t')) {
            line_end--;
        }

        size_t line_len = line_end - line_start;
        if (line_len == 0u) {
            continue;
        }
        if (line_len >= RATA_PROJECT_MAX_LINE) {
            rata_arena_restore(arena, mark);
            return RATA_ERR_FORMAT;
        }

        char line[RATA_PROJECT_MAX_LINE];
        memcpy(line, text + line_start, line_len);
        line[line_len] = '\0';

        /* Reject control characters in non-empty lines. */
        for (size_t i = 0u; i < line_len; ++i) {
            unsigned char c = (unsigned char)line[i];
            if (c < 0x20u || c == 0x7Fu) {
                rata_arena_restore(arena, mark);
                return RATA_ERR_FORMAT;
            }
        }

        if (line[0] == '#') {
            continue;
        }

        if (!saw_header) {
            const char *prefix = "RATA_PROJECT ";
            size_t plen = strlen(prefix);
            if (line_len <= plen || strncmp(line, prefix, plen) != 0) {
                rata_arena_restore(arena, mark);
                return RATA_ERR_FORMAT;
            }
            char *end = NULL;
            unsigned long ver = strtoul(line + plen, &end, 10);
            if (end == line + plen || *end != '\0' || ver != RATA_PROJECT_FORMAT_VERSION) {
                rata_arena_restore(arena, mark);
                return RATA_ERR_FORMAT;
            }
            doc_out->version = (uint32_t)ver;
            saw_header = true;
            continue;
        }

        if (line[0] == '[') {
            if (line[line_len - 1u] != ']' || line_len < 3u ||
                line_len - 2u >= RATA_PROJECT_SECTION_MAX) {
                rata_arena_restore(arena, mark);
                return RATA_ERR_PARSE;
            }
            char sec[RATA_PROJECT_SECTION_MAX];
            memcpy(sec, line + 1u, line_len - 2u);
            sec[line_len - 2u] = '\0';
            if (!is_valid_ident_str(sec, RATA_PROJECT_SECTION_MAX)) {
                rata_arena_restore(arena, mark);
                return RATA_ERR_PARSE;
            }
            memcpy(cur_section, sec, line_len - 1u);
            continue;
        }

        /* Parse `key value` (or `key` with empty value). */
        size_t sp = 0u;
        while (sp < line_len && line[sp] != ' ' && line[sp] != '\t') {
            sp++;
        }
        if (sp == 0u || sp >= RATA_PROJECT_KEY_MAX) {
            rata_arena_restore(arena, mark);
            return RATA_ERR_PARSE;
        }
        char key[RATA_PROJECT_KEY_MAX];
        memcpy(key, line, sp);
        key[sp] = '\0';
        if (!is_valid_ident_str(key, RATA_PROJECT_KEY_MAX)) {
            rata_arena_restore(arena, mark);
            return RATA_ERR_PARSE;
        }

        size_t val_start = sp;
        while (val_start < line_len && (line[val_start] == ' ' || line[val_start] == '\t')) {
            val_start++;
        }
        const char *val = line + val_start;
        if (strlen(val) >= RATA_PROJECT_VALUE_MAX) {
            rata_arena_restore(arena, mark);
            return RATA_ERR_PARSE;
        }

        st = project_doc_set(doc_out, cur_section, key, val);
        if (st != RATA_OK) {
            rata_arena_restore(arena, mark);
            return st;
        }
    }

    if (!saw_header) {
        rata_arena_restore(arena, mark);
        memset(doc_out, 0, sizeof(*doc_out));
        return RATA_ERR_FORMAT;
    }

    size_t buf_off = (size_t)((uint8_t *)doc_out->entries - arena->base);
    size_t used_entries = (doc_out->count > 0u) ? doc_out->count : 1u;
    rata_arena_restore(arena, buf_off + used_entries * sizeof(RataProjectEntry));
    doc_out->cap = used_entries;
    return RATA_OK;
}

static bool append_str(char *buf, size_t cap, size_t *pos, const char *s) {
    size_t slen = strlen(s);
    if (*pos + slen >= cap) {
        return false;
    }
    memcpy(buf + *pos, s, slen);
    *pos += slen;
    buf[*pos] = '\0';
    return true;
}

RataStatus project_file_serialize(const RataProjectDoc *doc,
                                  char *buf,
                                  size_t buf_cap,
                                  size_t *written_out) {
    if (doc == NULL || buf == NULL || buf_cap < 32u) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t pos = 0u;
    buf[0] = '\0';

    char header[64];
    snprintf(header, sizeof(header), "RATA_PROJECT %u\n", (unsigned)doc->version);
    if (!append_str(buf, buf_cap, &pos, header)) {
        return RATA_ERR_RANGE;
    }

    char last_section[RATA_PROJECT_SECTION_MAX];
    last_section[0] = '\0';

    for (size_t i = 0u; i < doc->count; ++i) {
        const RataProjectEntry *e = &doc->entries[i];
        if (strcmp(e->section, last_section) != 0) {
            if (e->section[0] != '\0') {
                char sec_line[RATA_PROJECT_SECTION_MAX + 8u];
                snprintf(sec_line, sizeof(sec_line), "\n[%s]\n", e->section);
                if (!append_str(buf, buf_cap, &pos, sec_line)) {
                    return RATA_ERR_RANGE;
                }
            }
            snprintf(last_section, sizeof(last_section), "%s", e->section);
        }
        char line[RATA_PROJECT_KEY_MAX + RATA_PROJECT_VALUE_MAX + 8u];
        if (e->value[0] != '\0') {
            snprintf(line, sizeof(line), "%s %s\n", e->key, e->value);
        } else {
            snprintf(line, sizeof(line), "%s\n", e->key);
        }
        if (!append_str(buf, buf_cap, &pos, line)) {
            return RATA_ERR_RANGE;
        }
    }

    if (written_out != NULL) {
        *written_out = pos;
    }
    return RATA_OK;
}

RataStatus project_file_save_path(const RataProjectDoc *doc, const char *path) {
    if (doc == NULL || path == NULL || path[0] == '\0') {
        return RATA_ERR_INVALID_ARG;
    }
    char buf[65536];
    size_t written = 0u;
    RataStatus st = project_file_serialize(doc, buf, sizeof(buf), &written);
    if (st != RATA_OK) {
        return st;
    }
    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        return RATA_ERR_IO;
    }
    size_t n = fwrite(buf, 1u, written, fp);
    int close_res = fclose(fp);
    if (n != written || close_res != 0) {
        return RATA_ERR_IO;
    }
    return RATA_OK;
}

RataStatus project_file_load_path(const char *path, RataProjectDoc *doc_out, RataArena *arena) {
    if (path == NULL || path[0] == '\0' || doc_out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return RATA_ERR_IO;
    }
    char buf[65536];
    size_t n = fread(buf, 1u, sizeof(buf) - 1u, fp);
    bool eof_ok = (feof(fp) != 0);
    fclose(fp);
    if (!eof_ok && n == sizeof(buf) - 1u) {
        return RATA_ERR_FORMAT;
    }
    buf[n] = '\0';
    return project_file_parse(buf, n, doc_out, arena);
}
