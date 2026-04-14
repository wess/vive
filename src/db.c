#include "vive.h"

int vive_db_open(const char *path, sqlite3 **out) {
    int rc = sqlite3_open(path, out);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db error: %s\n", sqlite3_errmsg(*out));
        sqlite3_close(*out);
        *out = NULL;
        return -1;
    }
    return vive_schema_init(*out);
}

int vive_db_open_readonly(const char *path, sqlite3 **out) {
    int rc = sqlite3_open_v2(path, out, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "db error: %s\n", sqlite3_errmsg(*out));
        sqlite3_close(*out);
        *out = NULL;
        return -1;
    }
    return 0;
}

void vive_db_close(sqlite3 *db) { if (db) sqlite3_close(db); }

/* ── Tasks ─────────────────────────────────────────────── */

int db_create_task(sqlite3 *db, Task *t) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO tasks (id,name,state,strategy,token_budget,tokens_used,created_at,updated_at)"
        " VALUES (?,?,?,?,?,?,?,?)", -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,t->id,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,t->name,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,3,task_state_str(t->state),-1,SQLITE_STATIC);
    sqlite3_bind_text(s,4,exec_strategy_str(t->strategy),-1,SQLITE_STATIC);
    sqlite3_bind_int(s,5,t->token_budget);
    sqlite3_bind_int(s,6,t->tokens_used);
    sqlite3_bind_int(s,7,t->created_at);
    sqlite3_bind_int(s,8,t->updated_at);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_update_task_state(sqlite3 *db, const char *id, TaskState state, const char *error) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE tasks SET state=?,error_reason=?,updated_at=? WHERE id=?",
        -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,task_state_str(state),-1,SQLITE_STATIC);
    if (error) sqlite3_bind_text(s,2,error,-1,SQLITE_STATIC); else sqlite3_bind_null(s,2);
    sqlite3_bind_int(s,3,vive_now());
    sqlite3_bind_text(s,4,id,-1,SQLITE_STATIC);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_update_task_tokens(sqlite3 *db, const char *id, int tokens) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE tasks SET tokens_used=?,updated_at=? WHERE id=?",
        -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(s,1,tokens); sqlite3_bind_int(s,2,vive_now());
    sqlite3_bind_text(s,3,id,-1,SQLITE_STATIC);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ── Agents ────────────────────────────────────────────── */

int db_register_agent(sqlite3 *db, Agent *a) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO agents (id,role,state,token_budget,tokens_used,registered_at)"
        " VALUES (?,?,'idle',?,?,?)", -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,a->id,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,a->role,-1,SQLITE_STATIC);
    sqlite3_bind_int(s,3,a->token_budget);
    sqlite3_bind_int(s,4,a->tokens_used);
    sqlite3_bind_int(s,5,vive_now());
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_update_agent_state(sqlite3 *db, const char *id, AgentState state, const char *task_id) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE agents SET state=?,current_task_id=? WHERE id=?",
        -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,agent_state_str(state),-1,SQLITE_STATIC);
    if (task_id) sqlite3_bind_text(s,2,task_id,-1,SQLITE_STATIC); else sqlite3_bind_null(s,2);
    sqlite3_bind_text(s,3,id,-1,SQLITE_STATIC);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ── Memory ────────────────────────────────────────────── */

int db_store_memory(sqlite3 *db, Memory *m) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO memories (id,content,kind,importance,access_count,ttl,created_at,last_accessed,session_id)"
        " VALUES (?,?,?,?,?,?,?,?,?)", -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,m->id,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,m->content,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,3,memory_kind_str(m->kind),-1,SQLITE_STATIC);
    sqlite3_bind_double(s,4,m->importance);
    sqlite3_bind_int(s,5,m->access_count);
    if (m->ttl > 0) sqlite3_bind_int(s,6,m->ttl); else sqlite3_bind_null(s,6);
    sqlite3_bind_int(s,7,m->created_at);
    sqlite3_bind_int(s,8,m->last_accessed);
    if (m->session_id[0]) sqlite3_bind_text(s,9,m->session_id,-1,SQLITE_STATIC);
    else sqlite3_bind_null(s,9);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_forget_memory(sqlite3 *db, const char *id) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM memories WHERE id=?", -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,id,-1,SQLITE_STATIC);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_bump_memory_access(sqlite3 *db, const char *id) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE memories SET access_count=access_count+1,last_accessed=? WHERE id=?",
        -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(s,1,vive_now()); sqlite3_bind_text(s,2,id,-1,SQLITE_STATIC);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_prune_expired(sqlite3 *db) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM memories WHERE ttl IS NOT NULL AND (created_at+ttl)<?",
        -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(s,1,vive_now());
    sqlite3_step(s); sqlite3_finalize(s);
    return 0;
}

/* ── Sessions ──────────────────────────────────────────── */

int db_create_session(sqlite3 *db, const char *id, const char *client) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "INSERT INTO sessions (id,client,connected_at,request_count) VALUES (?,?,?,0)",
        -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,id,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,client,-1,SQLITE_STATIC);
    sqlite3_bind_int(s,3,vive_now());
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_log_request(sqlite3 *db, const char *session_id, const char *method, int latency_ms, const char *error) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO request_log (session_id,method,latency_ms,error,created_at) VALUES (?,?,?,?,?)",
        -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,session_id,-1,SQLITE_STATIC);
    sqlite3_bind_text(s,2,method,-1,SQLITE_STATIC);
    sqlite3_bind_int(s,3,latency_ms);
    if (error) sqlite3_bind_text(s,4,error,-1,SQLITE_STATIC); else sqlite3_bind_null(s,4);
    sqlite3_bind_int(s,5,vive_now());
    sqlite3_step(s); sqlite3_finalize(s);

    if (sqlite3_prepare_v2(db, "UPDATE sessions SET request_count=request_count+1 WHERE id=?",
        -1, &s, NULL) == SQLITE_OK) {
        sqlite3_bind_text(s,1,session_id,-1,SQLITE_STATIC);
        sqlite3_step(s); sqlite3_finalize(s);
    }
    return 0;
}

/* ── Context Windows ───────────────────────────────────── */

int db_set_context_window(sqlite3 *db, ContextWindow *cw) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT OR REPLACE INTO context_windows (task_id,budget,used,compressed_size,full_size)"
        " VALUES (?,?,?,?,?)", -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,cw->task_id,-1,SQLITE_STATIC);
    sqlite3_bind_int(s,2,cw->budget);
    sqlite3_bind_int(s,3,cw->used);
    sqlite3_bind_int(s,4,cw->compressed_size);
    sqlite3_bind_int(s,5,cw->full_size);
    int rc = sqlite3_step(s); sqlite3_finalize(s);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ── Stats ─────────────────────────────────────────────── */

int db_query_stats(sqlite3 *db, Stats *st) {
    memset(st, 0, sizeof(Stats));
    sqlite3_stmt *s = NULL;

    if (sqlite3_prepare_v2(db, "SELECT state,COUNT(*) FROM tasks GROUP BY state", -1, &s, NULL) == SQLITE_OK) {
        while (sqlite3_step(s) == SQLITE_ROW) {
            const char *v = (const char *)sqlite3_column_text(s,0);
            int n = sqlite3_column_int(s,1);
            if      (strcmp(v,"pending")==0)   st->tasks_pending=n;
            else if (strcmp(v,"running")==0)   st->tasks_running=n;
            else if (strcmp(v,"completed")==0) st->tasks_completed=n;
            else if (strcmp(v,"failed")==0)    st->tasks_failed=n;
        }
        sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(db, "SELECT state,COUNT(*) FROM agents GROUP BY state", -1, &s, NULL) == SQLITE_OK) {
        while (sqlite3_step(s) == SQLITE_ROW) {
            const char *v = (const char *)sqlite3_column_text(s,0);
            int n = sqlite3_column_int(s,1);
            st->total_agents += n;
            if (strcmp(v,"idle")==0) st->agents_idle=n;
            else if (strcmp(v,"busy")==0) st->agents_busy=n;
        }
        sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(SUM(token_budget),0),COALESCE(SUM(tokens_used),0) FROM tasks WHERE state='running'",
        -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s)==SQLITE_ROW) { st->total_token_budget=sqlite3_column_int(s,0); st->total_tokens_used=sqlite3_column_int(s,1); }
        sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(db, "SELECT COALESCE(SUM(compressed_size),0) FROM context_windows", -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s)==SQLITE_ROW) st->total_compressed=sqlite3_column_int(s,0);
        sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(db, "SELECT kind,COUNT(*) FROM memories GROUP BY kind", -1, &s, NULL) == SQLITE_OK) {
        while (sqlite3_step(s)==SQLITE_ROW) {
            const char *k=(const char *)sqlite3_column_text(s,0); int n=sqlite3_column_int(s,1);
            st->total_memories+=n;
            if (strcmp(k,"semantic")==0) st->semantic_memories=n;
            else if (strcmp(k,"structured")==0) st->structured_memories=n;
            else if (strcmp(k,"session")==0) st->session_memories=n;
        }
        sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM sessions WHERE disconnected_at IS NULL", -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s)==SQLITE_ROW) st->connected_clients=sqlite3_column_int(s,0);
        sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*),COALESCE(AVG(latency_ms),0) FROM request_log", -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s)==SQLITE_ROW) { st->total_requests=sqlite3_column_int(s,0); st->avg_latency=(float)sqlite3_column_double(s,1); }
        sqlite3_finalize(s);
    }
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM request_log WHERE error IS NOT NULL", -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s)==SQLITE_ROW) st->total_errors=sqlite3_column_int(s,0);
        sqlite3_finalize(s);
    }
    return 0;
}
