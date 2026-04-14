#include "vive.h"
#include <unistd.h>

/* ── deduplication ─────────────────────────────────────── */

int mem_dedup(sqlite3 *db) {
    /* roll up duplicate file edit memories into latest only */
    sqlite3_stmt *s = NULL;
    const char *sql =
        "DELETE FROM memories WHERE id NOT IN ("
        "  SELECT MAX(id) FROM memories"
        "  WHERE kind='structured' AND (content LIKE 'created %' OR content LIKE 'modified %')"
        "  GROUP BY content"
        ") AND kind='structured' AND (content LIKE 'created %' OR content LIKE 'modified %')";
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_step(s);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(s);
    return changes;
}

/* ── CLI subcommands ───────────────────────────────────── */

void mem_list(sqlite3 *db, const char *kind, int limit) {
    if (limit <= 0) limit = 20;

    sqlite3_stmt *s = NULL;
    const char *sql = kind
        ? "SELECT id, content, kind, importance, access_count, ttl, created_at FROM memories"
          " WHERE kind=? ORDER BY importance DESC, access_count DESC LIMIT ?"
        : "SELECT id, content, kind, importance, access_count, ttl, created_at FROM memories"
          " ORDER BY importance DESC, access_count DESC LIMIT ?";
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK) {
        fprintf(stderr, "error: query failed\n");
        return;
    }
    if (kind) { sqlite3_bind_text(s, 1, kind, -1, SQLITE_STATIC); sqlite3_bind_int(s, 2, limit); }
    else { sqlite3_bind_int(s, 1, limit); }

    int count = 0;
    while (sqlite3_step(s) == SQLITE_ROW) {
        const char *id      = (const char *)sqlite3_column_text(s, 0);
        const char *content = (const char *)sqlite3_column_text(s, 1);
        const char *k       = (const char *)sqlite3_column_text(s, 2);
        float imp           = (float)sqlite3_column_double(s, 3);
        int acc             = sqlite3_column_int(s, 4);
        int ttl             = sqlite3_column_int(s, 5);

        /* truncate long content */
        char line[81];
        strncpy(line, content ? content : "", 80);
        line[80] = '\0';
        /* replace newlines */
        for (int i = 0; line[i]; i++) if (line[i] == '\n') line[i] = ' ';

        printf("%.8s  %-11s  imp=%.1f  acc=%-3d  %s%s\n",
            id ? id : "????????",
            k ? k : "?",
            imp, acc,
            line,
            ttl > 0 ? "  [expires]" : "");
        count++;
    }
    sqlite3_finalize(s);

    if (count == 0) printf("no memories found\n");
    else printf("\n%d memories shown\n", count);
}

void mem_search(sqlite3 *db, const char *text, int limit) {
    if (limit <= 0) limit = 20;
    cJSON *results = mem_query(db, text, limit);
    if (!results) { fprintf(stderr, "error: search failed\n"); return; }

    int n = cJSON_GetArraySize(results);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(results, i);
        const char *id = NULL, *content = NULL, *kind = NULL;
        cJSON *j;
        j = cJSON_GetObjectItem(item, "id"); if (j) id = j->valuestring;
        j = cJSON_GetObjectItem(item, "content"); if (j) content = j->valuestring;
        j = cJSON_GetObjectItem(item, "kind"); if (j) kind = j->valuestring;

        char line[81];
        strncpy(line, content ? content : "", 80);
        line[80] = '\0';
        for (int c = 0; line[c]; c++) if (line[c] == '\n') line[c] = ' ';

        printf("%.8s  %-11s  %s\n", id ? id : "????????", kind ? kind : "?", line);
    }
    cJSON_Delete(results);

    if (n == 0) printf("no matches\n");
    else printf("\n%d matches\n", n);
}

void mem_forget_cli(sqlite3 *db, const char *id) {
    if (db_forget_memory(db, id) == 0) printf("forgotten: %s\n", id);
    else fprintf(stderr, "error: memory not found or delete failed\n");
}

void mem_clear_all(sqlite3 *db, const char *kind) {
    sqlite3_stmt *s = NULL;
    const char *sql = kind
        ? "DELETE FROM memories WHERE kind=?"
        : "DELETE FROM memories";
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK) {
        fprintf(stderr, "error: clear failed\n");
        return;
    }
    if (kind) sqlite3_bind_text(s, 1, kind, -1, SQLITE_STATIC);
    sqlite3_step(s);
    int changes = sqlite3_changes(db);
    sqlite3_finalize(s);
    printf("cleared %d memories%s%s\n", changes, kind ? " (kind: " : "", kind ? kind : "");
    if (kind) printf(")\n");
}
