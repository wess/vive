#include "vive.h"

int route_create(sqlite3 *db, const char *name, const char *strategy_str, int token_budget, Task *out) {
    memset(out, 0, sizeof(Task));
    vive_gen_id(out->id);
    strncpy(out->name, name, sizeof(out->name) - 1);
    out->state        = TASK_PENDING;
    out->strategy     = exec_strategy_parse(strategy_str);
    out->token_budget = token_budget;
    out->created_at   = vive_now();
    out->updated_at   = vive_now();
    if (db_create_task(db, out) != 0) return -1;

    ContextWindow cw = {0};
    strncpy(cw.task_id, out->id, 16);
    cw.budget = token_budget;
    db_set_context_window(db, &cw);
    return 0;
}

int route_run(sqlite3 *db, const char *task_id) {
    return db_update_task_state(db, task_id, TASK_RUNNING, NULL);
}

cJSON *route_status(sqlite3 *db, const char *task_id) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT id,name,state,strategy,token_budget,tokens_used,error_reason,created_at,updated_at"
        " FROM tasks WHERE id=?", -1, &s, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(s,1,task_id,-1,SQLITE_STATIC);
    if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return NULL; }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj,"id",(const char*)sqlite3_column_text(s,0));
    cJSON_AddStringToObject(obj,"name",(const char*)sqlite3_column_text(s,1));
    cJSON_AddStringToObject(obj,"state",(const char*)sqlite3_column_text(s,2));
    cJSON_AddStringToObject(obj,"strategy",(const char*)sqlite3_column_text(s,3));
    cJSON_AddNumberToObject(obj,"token_budget",sqlite3_column_int(s,4));
    cJSON_AddNumberToObject(obj,"tokens_used",sqlite3_column_int(s,5));
    const char *er=(const char*)sqlite3_column_text(s,6);
    if (er) cJSON_AddStringToObject(obj,"error_reason",er);
    cJSON_AddNumberToObject(obj,"created_at",sqlite3_column_int(s,7));
    cJSON_AddNumberToObject(obj,"updated_at",sqlite3_column_int(s,8));
    sqlite3_finalize(s);

    if (sqlite3_prepare_v2(db,
        "SELECT id,step_index,state,description FROM task_steps WHERE task_id=? ORDER BY step_index",
        -1, &s, NULL) == SQLITE_OK) {
        sqlite3_bind_text(s,1,task_id,-1,SQLITE_STATIC);
        cJSON *steps = cJSON_CreateArray();
        while (sqlite3_step(s) == SQLITE_ROW) {
            cJSON *st = cJSON_CreateObject();
            cJSON_AddStringToObject(st,"id",(const char*)sqlite3_column_text(s,0));
            cJSON_AddNumberToObject(st,"step_index",sqlite3_column_int(s,1));
            cJSON_AddStringToObject(st,"state",(const char*)sqlite3_column_text(s,2));
            const char *d=(const char*)sqlite3_column_text(s,3);
            if (d) cJSON_AddStringToObject(st,"description",d);
            cJSON_AddItemToArray(steps, st);
        }
        sqlite3_finalize(s);
        cJSON_AddItemToObject(obj,"steps",steps);
    }
    return obj;
}

int route_cancel(sqlite3 *db, const char *task_id) {
    return db_update_task_state(db, task_id, TASK_CANCELLED, NULL);
}

cJSON *route_list(sqlite3 *db, const char *filter) {
    sqlite3_stmt *s = NULL;
    int rc;
    if (filter && strlen(filter) > 0) {
        rc = sqlite3_prepare_v2(db,
            "SELECT id,name,state,strategy,token_budget,tokens_used,created_at,updated_at"
            " FROM tasks WHERE state=? ORDER BY updated_at DESC", -1, &s, NULL);
        if (rc == SQLITE_OK) sqlite3_bind_text(s,1,filter,-1,SQLITE_STATIC);
    } else {
        rc = sqlite3_prepare_v2(db,
            "SELECT id,name,state,strategy,token_budget,tokens_used,created_at,updated_at"
            " FROM tasks ORDER BY updated_at DESC", -1, &s, NULL);
    }
    if (rc != SQLITE_OK) return NULL;

    cJSON *arr = cJSON_CreateArray();
    while (sqlite3_step(s) == SQLITE_ROW) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddStringToObject(o,"id",(const char*)sqlite3_column_text(s,0));
        cJSON_AddStringToObject(o,"name",(const char*)sqlite3_column_text(s,1));
        cJSON_AddStringToObject(o,"state",(const char*)sqlite3_column_text(s,2));
        cJSON_AddStringToObject(o,"strategy",(const char*)sqlite3_column_text(s,3));
        cJSON_AddNumberToObject(o,"token_budget",sqlite3_column_int(s,4));
        cJSON_AddNumberToObject(o,"tokens_used",sqlite3_column_int(s,5));
        cJSON_AddNumberToObject(o,"created_at",sqlite3_column_int(s,6));
        cJSON_AddNumberToObject(o,"updated_at",sqlite3_column_int(s,7));
        cJSON_AddItemToArray(arr, o);
    }
    sqlite3_finalize(s);
    return arr;
}

int route_register_agent(sqlite3 *db, const char *role, int token_budget, Agent *out) {
    memset(out, 0, sizeof(Agent));
    vive_gen_id(out->id);
    strncpy(out->role, role, sizeof(out->role) - 1);
    out->state        = AGENT_IDLE;
    out->token_budget = token_budget;
    return db_register_agent(db, out);
}

int route_handoff(sqlite3 *db, const char *from_id, const char *to_id) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db, "SELECT current_task_id FROM agents WHERE id=?", -1, &s, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(s,1,from_id,-1,SQLITE_STATIC);
    char tid[17] = {0};
    if (sqlite3_step(s) == SQLITE_ROW) {
        const char *t = (const char *)sqlite3_column_text(s,0);
        if (t) strncpy(tid, t, 16);
    }
    sqlite3_finalize(s);
    db_update_agent_state(db, from_id, AGENT_IDLE, NULL);
    if (tid[0]) db_update_agent_state(db, to_id, AGENT_BUSY, tid);
    return 0;
}

int route_spawn_agent(sqlite3 *db, const char *role, const char *task_id, Agent *out) {
    memset(out, 0, sizeof(Agent));
    vive_gen_id(out->id);
    strncpy(out->role, role, sizeof(out->role) - 1);
    out->state = AGENT_BUSY;
    strncpy(out->current_task_id, task_id, 16);
    if (db_register_agent(db, out) != 0) return -1;
    return db_update_agent_state(db, out->id, AGENT_BUSY, task_id);
}

cJSON *route_agent_status(sqlite3 *db, const char *agent_id) {
    sqlite3_stmt *s = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT id,role,state,current_task_id,token_budget,tokens_used FROM agents WHERE id=?",
        -1, &s, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_text(s,1,agent_id,-1,SQLITE_STATIC);
    if (sqlite3_step(s) != SQLITE_ROW) { sqlite3_finalize(s); return NULL; }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj,"id",(const char*)sqlite3_column_text(s,0));
    cJSON_AddStringToObject(obj,"role",(const char*)sqlite3_column_text(s,1));
    cJSON_AddStringToObject(obj,"state",(const char*)sqlite3_column_text(s,2));
    const char *t=(const char*)sqlite3_column_text(s,3);
    if (t) cJSON_AddStringToObject(obj,"task_id",t);
    cJSON_AddNumberToObject(obj,"token_budget",sqlite3_column_int(s,4));
    cJSON_AddNumberToObject(obj,"tokens_used",sqlite3_column_int(s,5));
    sqlite3_finalize(s);
    return obj;
}
