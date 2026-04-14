#include "vive.h"

int mem_store(sqlite3 *db, const char *content, const char *kind_str, int ttl, Memory *out) {
    memset(out, 0, sizeof(Memory));
    vive_gen_id(out->id);
    out->content       = strdup(content);
    out->kind          = memory_kind_parse(kind_str);
    out->ttl           = ttl;
    out->created_at    = vive_now();
    out->last_accessed = vive_now();
    return db_store_memory(db, out);
}

cJSON *mem_query(sqlite3 *db, const char *text, int limit) {
    if (limit <= 0) limit = 10;
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT id,content,kind,importance,access_count,ttl,created_at,last_accessed"
        " FROM memories WHERE content LIKE ? ORDER BY importance DESC,access_count DESC LIMIT ?",
        -1, &s, NULL) != SQLITE_OK) return NULL;

    size_t tl = strlen(text);
    char *pat = malloc(tl + 3);
    pat[0] = '%'; memcpy(pat+1, text, tl); pat[tl+1] = '%'; pat[tl+2] = '\0';
    sqlite3_bind_text(s,1,pat,-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(s,2,limit);

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(s) == SQLITE_ROW) {
        Memory m = {0};
        strncpy(m.id, (const char *)sqlite3_column_text(s,0), 16);
        m.content       = strdup((const char *)sqlite3_column_text(s,1));
        m.kind          = memory_kind_parse((const char *)sqlite3_column_text(s,2));
        m.importance    = (float)sqlite3_column_double(s,3);
        m.access_count  = sqlite3_column_int(s,4);
        m.ttl           = sqlite3_column_int(s,5);
        m.created_at    = sqlite3_column_int(s,6);
        m.last_accessed = sqlite3_column_int(s,7);
        db_bump_memory_access(db, m.id);
        cJSON_AddItemToArray(arr, memory_to_json(&m));
        free(m.content);
    }
    sqlite3_finalize(s);
    free(pat);
    return arr;
}

char *mem_summarize(sqlite3 *db, cJSON *ids) {
    if (!ids || !cJSON_IsArray(ids)) return NULL;
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    buf[0] = '\0';

    int n = cJSON_GetArraySize(ids);
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(ids, i);
        if (!item || !cJSON_IsString(item)) continue;
        sqlite3_stmt *s = NULL;
        if (sqlite3_prepare_v2(db, "SELECT content FROM memories WHERE id=?", -1, &s, NULL) != SQLITE_OK) continue;
        sqlite3_bind_text(s,1,item->valuestring,-1,SQLITE_STATIC);
        if (sqlite3_step(s) == SQLITE_ROW) {
            const char *c = (const char *)sqlite3_column_text(s,0);
            if (c) {
                size_t cl = strlen(c);
                while (len + cl + 2 >= cap) { cap *= 2; buf = realloc(buf, cap); }
                memcpy(buf + len, c, cl); len += cl;
                buf[len++] = '\n'; buf[len] = '\0';
            }
        }
        sqlite3_finalize(s);
    }
    return buf;
}

int mem_prune(sqlite3 *db) { return db_prune_expired(db); }

cJSON *mem_query_top(sqlite3 *db, const char *kind, int limit) {
    if (limit <= 0) limit = 10;
    sqlite3_stmt *s = NULL;
    const char *sql = kind
        ? "SELECT id,content,kind,importance,access_count,ttl,created_at,last_accessed"
          " FROM memories WHERE kind=? ORDER BY importance DESC,access_count DESC LIMIT ?"
        : "SELECT id,content,kind,importance,access_count,ttl,created_at,last_accessed"
          " FROM memories ORDER BY importance DESC,access_count DESC LIMIT ?";
    if (sqlite3_prepare_v2(db, sql, -1, &s, NULL) != SQLITE_OK) return NULL;
    if (kind) { sqlite3_bind_text(s,1,kind,-1,SQLITE_STATIC); sqlite3_bind_int(s,2,limit); }
    else { sqlite3_bind_int(s,1,limit); }

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(s) == SQLITE_ROW) {
        Memory m = {0};
        strncpy(m.id, (const char *)sqlite3_column_text(s,0), 16);
        m.content       = strdup((const char *)sqlite3_column_text(s,1));
        m.kind          = memory_kind_parse((const char *)sqlite3_column_text(s,2));
        m.importance    = (float)sqlite3_column_double(s,3);
        m.access_count  = sqlite3_column_int(s,4);
        m.ttl           = sqlite3_column_int(s,5);
        m.created_at    = sqlite3_column_int(s,6);
        m.last_accessed = sqlite3_column_int(s,7);
        db_bump_memory_access(db, m.id);
        cJSON_AddItemToArray(arr, memory_to_json(&m));
        free(m.content);
    }
    sqlite3_finalize(s);
    return arr;
}

cJSON *mem_query_by_session(sqlite3 *db, const char *session_id) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT id,content,kind,importance,access_count,ttl,created_at,last_accessed"
        " FROM memories WHERE session_id=? ORDER BY created_at ASC",
        -1, &s, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(s,1,session_id,-1,SQLITE_STATIC);

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(s) == SQLITE_ROW) {
        Memory m = {0};
        strncpy(m.id, (const char *)sqlite3_column_text(s,0), 16);
        m.content       = strdup((const char *)sqlite3_column_text(s,1));
        m.kind          = memory_kind_parse((const char *)sqlite3_column_text(s,2));
        m.importance    = (float)sqlite3_column_double(s,3);
        m.access_count  = sqlite3_column_int(s,4);
        m.ttl           = sqlite3_column_int(s,5);
        m.created_at    = sqlite3_column_int(s,6);
        m.last_accessed = sqlite3_column_int(s,7);
        cJSON_AddItemToArray(arr, memory_to_json(&m));
        free(m.content);
    }
    sqlite3_finalize(s);
    return arr;
}
