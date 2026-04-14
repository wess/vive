#include "vive.h"

int ctx_budget(sqlite3 *db, const char *task_id, int limit) {
    ContextWindow cw = {0};
    strncpy(cw.task_id, task_id, 16);
    cw.budget = limit;

    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "SELECT used,compressed_size,full_size FROM context_windows WHERE task_id=?",
            -1, &s, NULL) == SQLITE_OK) {
        sqlite3_bind_text(s,1,task_id,-1,SQLITE_STATIC);
        if (sqlite3_step(s) == SQLITE_ROW) {
            cw.used            = sqlite3_column_int(s,0);
            cw.compressed_size = sqlite3_column_int(s,1);
            cw.full_size       = sqlite3_column_int(s,2);
        }
        sqlite3_finalize(s);
    }
    return db_set_context_window(db, &cw);
}

int ctx_inject(sqlite3 *db, const char *task_id, int content_tokens) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE context_windows SET used=used+?,compressed_size=compressed_size+? WHERE task_id=?",
            -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(s,1,content_tokens);
    sqlite3_bind_int(s,2,content_tokens);
    sqlite3_bind_text(s,3,task_id,-1,SQLITE_STATIC);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int ctx_retrieve(sqlite3 *db, const char *task_id, ContextWindow *out) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT task_id,budget,used,compressed_size,full_size FROM context_windows WHERE task_id=?",
            -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,task_id,-1,SQLITE_STATIC);
    if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return -1; }
    memset(out, 0, sizeof(ContextWindow));
    strncpy(out->task_id, (const char *)sqlite3_column_text(s,0), 16);
    out->budget          = sqlite3_column_int(s,1);
    out->used            = sqlite3_column_int(s,2);
    out->compressed_size = sqlite3_column_int(s,3);
    out->full_size       = sqlite3_column_int(s,4);
    sqlite3_finalize(s);
    return 0;
}

char *ctx_compress(sqlite3 *db, const char *task_id) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
            "SELECT description FROM task_steps WHERE task_id=? AND state='completed' ORDER BY step_index",
            -1, &s, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(s,1,task_id,-1,SQLITE_STATIC);

    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    buf[0] = '\0';
    while (sqlite3_step(s) == SQLITE_ROW) {
        const char *d = (const char *)sqlite3_column_text(s,0);
        if (!d) continue;
        size_t dl = strlen(d);
        while (len + dl + 2 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        memcpy(buf + len, d, dl); len += dl;
        buf[len++] = '\n'; buf[len] = '\0';
    }
    sqlite3_finalize(s);
    return buf;
}
