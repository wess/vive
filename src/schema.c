#include "vive.h"

static const char *SCHEMA_SQL =
    "PRAGMA journal_mode=WAL;"
    "PRAGMA foreign_keys=ON;"

    "CREATE TABLE IF NOT EXISTS schema_version ("
    "  version INTEGER NOT NULL"
    ");"

    "CREATE TABLE IF NOT EXISTS tasks ("
    "  id TEXT PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  state TEXT NOT NULL DEFAULT 'pending',"
    "  strategy TEXT NOT NULL DEFAULT 'sequential',"
    "  token_budget INTEGER NOT NULL DEFAULT 0,"
    "  tokens_used INTEGER NOT NULL DEFAULT 0,"
    "  error_reason TEXT,"
    "  created_at INTEGER NOT NULL,"
    "  updated_at INTEGER NOT NULL"
    ");"

    "CREATE TABLE IF NOT EXISTS task_steps ("
    "  id TEXT PRIMARY KEY,"
    "  task_id TEXT NOT NULL REFERENCES tasks(id),"
    "  step_index INTEGER NOT NULL,"
    "  state TEXT NOT NULL DEFAULT 'pending',"
    "  description TEXT,"
    "  started_at INTEGER,"
    "  completed_at INTEGER"
    ");"

    "CREATE TABLE IF NOT EXISTS agents ("
    "  id TEXT PRIMARY KEY,"
    "  role TEXT NOT NULL,"
    "  state TEXT NOT NULL DEFAULT 'idle',"
    "  current_task_id TEXT REFERENCES tasks(id),"
    "  tools TEXT,"
    "  token_budget INTEGER NOT NULL DEFAULT 0,"
    "  tokens_used INTEGER NOT NULL DEFAULT 0,"
    "  error_reason TEXT,"
    "  registered_at INTEGER NOT NULL"
    ");"

    "CREATE TABLE IF NOT EXISTS memories ("
    "  id TEXT PRIMARY KEY,"
    "  content TEXT NOT NULL,"
    "  kind TEXT NOT NULL CHECK(kind IN ('semantic','structured','session')),"
    "  importance REAL NOT NULL DEFAULT 0.0,"
    "  access_count INTEGER NOT NULL DEFAULT 0,"
    "  ttl INTEGER,"
    "  created_at INTEGER NOT NULL,"
    "  last_accessed INTEGER NOT NULL"
    ");"

    "CREATE TABLE IF NOT EXISTS context_windows ("
    "  task_id TEXT PRIMARY KEY REFERENCES tasks(id),"
    "  budget INTEGER NOT NULL,"
    "  used INTEGER NOT NULL DEFAULT 0,"
    "  compressed_size INTEGER NOT NULL DEFAULT 0,"
    "  full_size INTEGER NOT NULL DEFAULT 0"
    ");"

    "CREATE TABLE IF NOT EXISTS sessions ("
    "  id TEXT PRIMARY KEY,"
    "  client TEXT NOT NULL,"
    "  connected_at INTEGER NOT NULL,"
    "  disconnected_at INTEGER,"
    "  request_count INTEGER NOT NULL DEFAULT 0"
    ");"

    "CREATE TABLE IF NOT EXISTS request_log ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  session_id TEXT REFERENCES sessions(id),"
    "  method TEXT NOT NULL,"
    "  latency_ms INTEGER NOT NULL,"
    "  error TEXT,"
    "  created_at INTEGER NOT NULL"
    ");"

    "CREATE INDEX IF NOT EXISTS idx_request_log_created ON request_log(created_at);"
    "CREATE INDEX IF NOT EXISTS idx_memories_kind ON memories(kind);"
    "CREATE INDEX IF NOT EXISTS idx_tasks_state ON tasks(state);"
    "CREATE INDEX IF NOT EXISTS idx_task_steps_task ON task_steps(task_id);"
;

int vive_schema_init(sqlite3 *db) {
    char *err = NULL;
    int rc = sqlite3_exec(db, SCHEMA_SQL, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "schema error: %s\n", err);
        sqlite3_free(err);
        return -1;
    }

    /* migrate: add session_id to memories if missing */
    sqlite3_stmt *chk = NULL;
    int has_session_id = 0;
    if (sqlite3_prepare_v2(db, "PRAGMA table_info(memories)", -1, &chk, NULL) == SQLITE_OK) {
        while (sqlite3_step(chk) == SQLITE_ROW) {
            const char *col = (const char *)sqlite3_column_text(chk, 1);
            if (col && strcmp(col, "session_id") == 0) has_session_id = 1;
        }
        sqlite3_finalize(chk);
    }
    if (!has_session_id) {
        sqlite3_exec(db,
            "ALTER TABLE memories ADD COLUMN session_id TEXT;"
            "CREATE INDEX IF NOT EXISTS idx_memories_session ON memories(session_id);",
            NULL, NULL, NULL);
    }

    return 0;
}
