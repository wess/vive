#include "vive.h"

static int req_counter = 0;
#define PRUNE_INTERVAL 100

static char *dispatch_tool(sqlite3 *db, cJSON *id, const char *name, cJSON *args) {
    if (!name) return proto_error(id, MCP_INVALID_PARAMS, "missing tool name");

    if (strcmp(name,"task.create")==0) {
        const char *n=proto_param_str(args,"name"), *s=proto_param_str(args,"strategy");
        int b=proto_param_int(args,"token_budget",0);
        if (!n) return proto_error(id, MCP_INVALID_PARAMS, "missing name");
        if (!s) s="sequential";
        Task t; if (route_create(db,n,s,b,&t)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"create failed");
        return proto_response(id, task_to_json(&t));
    }
    if (strcmp(name,"task.run")==0) {
        const char *tid=proto_param_str(args,"task_id");
        if (!tid) return proto_error(id, MCP_INVALID_PARAMS, "missing task_id");
        if (route_run(db,tid)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"run failed");
        cJSON *ok=cJSON_CreateObject(); cJSON_AddBoolToObject(ok,"ok",1); return proto_response(id,ok);
    }
    if (strcmp(name,"task.status")==0) {
        const char *tid=proto_param_str(args,"task_id");
        if (!tid) return proto_error(id, MCP_INVALID_PARAMS, "missing task_id");
        cJSON *r=route_status(db,tid); if (!r) return proto_error(id,MCP_INTERNAL_ERROR,"not found");
        return proto_response(id,r);
    }
    if (strcmp(name,"task.cancel")==0) {
        const char *tid=proto_param_str(args,"task_id");
        if (!tid) return proto_error(id, MCP_INVALID_PARAMS, "missing task_id");
        if (route_cancel(db,tid)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"cancel failed");
        cJSON *ok=cJSON_CreateObject(); cJSON_AddBoolToObject(ok,"ok",1); return proto_response(id,ok);
    }
    if (strcmp(name,"task.list")==0) {
        const char *f=proto_param_str(args,"filter");
        cJSON *r=route_list(db,f); if (!r) return proto_error(id,MCP_INTERNAL_ERROR,"list failed");
        cJSON *w=cJSON_CreateObject(); cJSON_AddItemToObject(w,"tasks",r); return proto_response(id,w);
    }
    if (strcmp(name,"memory.store")==0) {
        const char *c=proto_param_str(args,"content"), *k=proto_param_str(args,"kind");
        int ttl=proto_param_int(args,"ttl",0);
        if (!c) return proto_error(id, MCP_INVALID_PARAMS, "missing content");
        if (!k) k="structured";
        Memory m; if (mem_store(db,c,k,ttl,&m)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"store failed");
        char *resp=proto_response(id, memory_to_json(&m)); free(m.content); return resp;
    }
    if (strcmp(name,"memory.query")==0) {
        const char *text=proto_param_str(args,"text");
        int lim=proto_param_int(args,"limit",10);
        if (!text) return proto_error(id, MCP_INVALID_PARAMS, "missing text");
        cJSON *r=mem_query(db,text,lim); if (!r) return proto_error(id,MCP_INTERNAL_ERROR,"query failed");
        cJSON *w=cJSON_CreateObject(); cJSON_AddItemToObject(w,"memories",r); return proto_response(id,w);
    }
    if (strcmp(name,"memory.forget")==0) {
        const char *mid=proto_param_str(args,"id");
        if (!mid) return proto_error(id, MCP_INVALID_PARAMS, "missing id");
        if (db_forget_memory(db,mid)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"forget failed");
        cJSON *ok=cJSON_CreateObject(); cJSON_AddBoolToObject(ok,"ok",1); return proto_response(id,ok);
    }
    if (strcmp(name,"memory.summarize")==0) {
        cJSON *ids=cJSON_GetObjectItem(args,"ids");
        char *c=mem_summarize(db,ids); if (!c) return proto_error(id,MCP_INTERNAL_ERROR,"summarize failed");
        cJSON *r=cJSON_CreateObject(); cJSON_AddStringToObject(r,"content",c); free(c); return proto_response(id,r);
    }
    if (strcmp(name,"context.compress")==0) {
        const char *tid=proto_param_str(args,"task_id");
        if (!tid) return proto_error(id, MCP_INVALID_PARAMS, "missing task_id");
        char *c=ctx_compress(db,tid); if (!c) return proto_error(id,MCP_INTERNAL_ERROR,"compress failed");
        cJSON *r=cJSON_CreateObject(); cJSON_AddStringToObject(r,"content",c); free(c); return proto_response(id,r);
    }
    if (strcmp(name,"context.retrieve")==0) {
        const char *tid=proto_param_str(args,"task_id");
        if (!tid) return proto_error(id, MCP_INVALID_PARAMS, "missing task_id");
        ContextWindow cw; if (ctx_retrieve(db,tid,&cw)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"not found");
        cJSON *r=cJSON_CreateObject();
        cJSON_AddStringToObject(r,"task_id",cw.task_id);
        cJSON_AddNumberToObject(r,"budget",cw.budget);
        cJSON_AddNumberToObject(r,"used",cw.used);
        cJSON_AddNumberToObject(r,"compressed_size",cw.compressed_size);
        cJSON_AddNumberToObject(r,"full_size",cw.full_size);
        return proto_response(id,r);
    }
    if (strcmp(name,"context.budget")==0) {
        const char *tid=proto_param_str(args,"task_id");
        int lim=proto_param_int(args,"limit",0);
        if (!tid) return proto_error(id, MCP_INVALID_PARAMS, "missing task_id");
        if (ctx_budget(db,tid,lim)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"budget failed");
        cJSON *ok=cJSON_CreateObject(); cJSON_AddBoolToObject(ok,"ok",1); return proto_response(id,ok);
    }
    if (strcmp(name,"context.inject")==0) {
        const char *tid=proto_param_str(args,"task_id");
        int tokens=proto_param_int(args,"tokens",0);
        if (!tid) return proto_error(id, MCP_INVALID_PARAMS, "missing task_id");
        if (ctx_inject(db,tid,tokens)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"inject failed");
        cJSON *ok=cJSON_CreateObject(); cJSON_AddBoolToObject(ok,"ok",1); return proto_response(id,ok);
    }
    if (strcmp(name,"agent.register")==0) {
        const char *role=proto_param_str(args,"role");
        int b=proto_param_int(args,"token_budget",0);
        if (!role) return proto_error(id, MCP_INVALID_PARAMS, "missing role");
        Agent a; if (route_register_agent(db,role,b,&a)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"register failed");
        return proto_response(id, agent_to_json(&a));
    }
    if (strcmp(name,"agent.handoff")==0) {
        const char *from=proto_param_str(args,"from_agent_id"), *to=proto_param_str(args,"to_agent_id");
        if (!from||!to) return proto_error(id, MCP_INVALID_PARAMS, "missing agent ids");
        if (route_handoff(db,from,to)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"handoff failed");
        cJSON *ok=cJSON_CreateObject(); cJSON_AddBoolToObject(ok,"ok",1); return proto_response(id,ok);
    }
    if (strcmp(name,"agent.spawn")==0) {
        const char *role=proto_param_str(args,"role"), *tid=proto_param_str(args,"task_id");
        if (!role||!tid) return proto_error(id, MCP_INVALID_PARAMS, "missing role or task_id");
        Agent a; if (route_spawn_agent(db,role,tid,&a)!=0) return proto_error(id,MCP_INTERNAL_ERROR,"spawn failed");
        return proto_response(id, agent_to_json(&a));
    }
    if (strcmp(name,"agent.status")==0) {
        const char *aid=proto_param_str(args,"agent_id");
        if (!aid) return proto_error(id, MCP_INVALID_PARAMS, "missing agent_id");
        cJSON *r=route_agent_status(db,aid); if (!r) return proto_error(id,MCP_INTERNAL_ERROR,"not found");
        return proto_response(id,r);
    }
    return proto_error(id, MCP_METHOD_NOT_FOUND, "unknown tool");
}

static char *mcp_handle(sqlite3 *db, const char *sess, const char *method, cJSON *id, cJSON *params) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    char *resp = NULL;

    if (strcmp(method,"initialize")==0)                resp = proto_init_response(id);
    else if (strcmp(method,"notifications/initialized")==0) return NULL;
    else if (strcmp(method,"tools/list")==0)            resp = proto_tools_list(id);
    else if (strcmp(method,"tools/call")==0) {
        const char *tool = proto_param_str(params,"name");
        cJSON *args = cJSON_GetObjectItem(params,"arguments");
        resp = dispatch_tool(db, id, tool, args);
    } else resp = proto_error(id, MCP_METHOD_NOT_FOUND, "unknown method");

    clock_gettime(CLOCK_MONOTONIC, &t1);
    int lat = (int)((t1.tv_sec-t0.tv_sec)*1000 + (t1.tv_nsec-t0.tv_nsec)/1000000);
    db_log_request(db, sess, method, lat, NULL);

    if (++req_counter >= PRUNE_INTERVAL) { req_counter = 0; mem_prune(db); }
    return resp;
}

static char *read_line(void) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    for (;;) {
        int c = fgetc(stdin);
        if (c == EOF) { free(buf); return NULL; }
        if (c == '\n') { buf[len] = '\0'; return buf; }
        if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = (char)c;
    }
}

void mcp_stdio_loop(sqlite3 *db) {
    char sess[17];
    vive_gen_id(sess);
    db_create_session(db, sess, "stdio");

    char *line;
    while ((line = read_line()) != NULL) {
        cJSON *root = cJSON_Parse(line);
        free(line);
        if (!root) {
            char *e = proto_error(NULL, MCP_PARSE_ERROR, "invalid JSON");
            fprintf(stdout, "%s\n", e); fflush(stdout); free(e);
            continue;
        }
        cJSON *mj = cJSON_GetObjectItem(root,"method");
        cJSON *ij = cJSON_GetObjectItem(root,"id");
        cJSON *pj = cJSON_GetObjectItem(root,"params");
        if (!mj || !cJSON_IsString(mj)) {
            char *e = proto_error(ij, MCP_INVALID_REQUEST, "missing method");
            fprintf(stdout, "%s\n", e); fflush(stdout); free(e);
            cJSON_Delete(root); continue;
        }
        char *resp = mcp_handle(db, sess, mj->valuestring, ij, pj);
        if (resp) { fprintf(stdout, "%s\n", resp); fflush(stdout); free(resp); }
        cJSON_Delete(root);
    }
}
