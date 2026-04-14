#include "vive.h"
#include <stdarg.h>

char *proto_response(cJSON *id, cJSON *result) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    if (id) cJSON_AddItemToObject(root, "id", cJSON_Duplicate(id, 1));
    cJSON_AddItemToObject(root, "result", result);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

char *proto_error(cJSON *id, int code, const char *msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    if (id) cJSON_AddItemToObject(root, "id", cJSON_Duplicate(id, 1));
    cJSON *err = cJSON_CreateObject();
    cJSON_AddNumberToObject(err, "code", code);
    cJSON_AddStringToObject(err, "message", msg);
    cJSON_AddItemToObject(root, "error", err);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

char *proto_init_response(cJSON *id) {
    cJSON *result = cJSON_CreateObject();
    cJSON *info = cJSON_CreateObject();
    cJSON_AddStringToObject(info, "name", "vive");
    cJSON_AddStringToObject(info, "version", "0.1.0");
    cJSON_AddItemToObject(result, "serverInfo", info);
    cJSON *caps = cJSON_CreateObject();
    cJSON_AddItemToObject(caps, "tools", cJSON_CreateObject());
    cJSON_AddItemToObject(result, "capabilities", caps);
    cJSON_AddStringToObject(result, "protocolVersion", "2024-11-05");
    return proto_response(id, result);
}

static cJSON *make_prop(const char *type, const char *desc) {
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "type", type);
    cJSON_AddStringToObject(p, "description", desc);
    return p;
}

static cJSON *make_schema(const char *required[], int nreq, ...) {
    cJSON *s = cJSON_CreateObject();
    cJSON_AddStringToObject(s, "type", "object");
    cJSON *props = cJSON_CreateObject();

    va_list ap;
    va_start(ap, nreq);
    for (;;) {
        const char *name = va_arg(ap, const char *);
        if (!name) break;
        const char *type = va_arg(ap, const char *);
        const char *desc = va_arg(ap, const char *);
        cJSON_AddItemToObject(props, name, make_prop(type, desc));
    }
    va_end(ap);

    cJSON_AddItemToObject(s, "properties", props);
    if (required && nreq > 0) {
        cJSON *req = cJSON_CreateArray();
        for (int i = 0; i < nreq; i++)
            cJSON_AddItemToArray(req, cJSON_CreateString(required[i]));
        cJSON_AddItemToObject(s, "required", req);
    }
    return s;
}

char *proto_tools_list(cJSON *id) {
    cJSON *result = cJSON_CreateObject();
    cJSON *tools = cJSON_CreateArray();

    struct { const char *name; const char *desc; cJSON *schema; } defs[] = {
        {"task.create", "Create a new task",
         make_schema((const char*[]){"name"}, 1,
            "name","string","Task name",
            "strategy","string","sequential, parallel, supervised, or statemachine",
            "token_budget","number","Maximum tokens allocated",
            NULL)},
        {"task.run", "Execute a task",
         make_schema((const char*[]){"task_id"}, 1, "task_id","string","Task ID", NULL)},
        {"task.status", "Get task status with step progress",
         make_schema((const char*[]){"task_id"}, 1, "task_id","string","Task ID", NULL)},
        {"task.cancel", "Cancel a running task",
         make_schema((const char*[]){"task_id"}, 1, "task_id","string","Task ID", NULL)},
        {"task.list", "List tasks with optional state filter",
         make_schema(NULL, 0, "filter","string","Filter by state: pending, running, completed, failed", NULL)},
        {"memory.store", "Store a memory",
         make_schema((const char*[]){"content"}, 1,
            "content","string","Memory content",
            "kind","string","semantic, structured, or session",
            "ttl","number","Time-to-live in seconds (0 = permanent)",
            NULL)},
        {"memory.query", "Query memories by content",
         make_schema((const char*[]){"text"}, 1,
            "text","string","Search text",
            "limit","number","Max results (default 10)",
            NULL)},
        {"memory.forget", "Delete a memory",
         make_schema((const char*[]){"id"}, 1, "id","string","Memory ID", NULL)},
        {"memory.summarize", "Get raw content of memories for client-side summarization",
         make_schema((const char*[]){"ids"}, 1, "ids","array","Array of memory IDs", NULL)},
        {"context.compress", "Get completed step content for client-side compression",
         make_schema((const char*[]){"task_id"}, 1, "task_id","string","Task ID", NULL)},
        {"context.retrieve", "Get context window statistics",
         make_schema((const char*[]){"task_id"}, 1, "task_id","string","Task ID", NULL)},
        {"context.budget", "Set token budget for a task",
         make_schema((const char*[]){"task_id","limit"}, 2,
            "task_id","string","Task ID",
            "limit","number","Token budget",
            NULL)},
        {"context.inject", "Add compressed context back to a task",
         make_schema((const char*[]){"task_id","tokens"}, 2,
            "task_id","string","Task ID",
            "tokens","number","Token count of injected content",
            NULL)},
        {"agent.register", "Register a new agent",
         make_schema((const char*[]){"role"}, 1,
            "role","string","Agent role (e.g. code-reviewer, planner)",
            "token_budget","number","Token budget for this agent",
            NULL)},
        {"agent.handoff", "Transfer task between agents",
         make_schema((const char*[]){"from_agent_id","to_agent_id"}, 2,
            "from_agent_id","string","Source agent ID",
            "to_agent_id","string","Target agent ID",
            NULL)},
        {"agent.spawn", "Create agent assigned to a task",
         make_schema((const char*[]){"role","task_id"}, 2,
            "role","string","Agent role",
            "task_id","string","Task to assign",
            NULL)},
        {"agent.status", "Get agent status",
         make_schema((const char*[]){"agent_id"}, 1, "agent_id","string","Agent ID", NULL)},
    };

    for (int i = 0; i < 17; i++) {
        cJSON *t = cJSON_CreateObject();
        cJSON_AddStringToObject(t, "name", defs[i].name);
        cJSON_AddStringToObject(t, "description", defs[i].desc);
        cJSON_AddItemToObject(t, "inputSchema", defs[i].schema);
        cJSON_AddItemToArray(tools, t);
    }
    cJSON_AddItemToObject(result, "tools", tools);
    return proto_response(id, result);
}

const char *proto_param_str(cJSON *params, const char *key) {
    if (!params) return NULL;
    cJSON *item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsString(item)) return NULL;
    return item->valuestring;
}

int proto_param_int(cJSON *params, const char *key, int def) {
    if (!params) return def;
    cJSON *item = cJSON_GetObjectItem(params, key);
    if (!item || !cJSON_IsNumber(item)) return def;
    return item->valueint;
}

cJSON *task_to_json(Task *t) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", t->id);
    cJSON_AddStringToObject(obj, "name", t->name);
    cJSON_AddStringToObject(obj, "state", task_state_str(t->state));
    cJSON_AddStringToObject(obj, "strategy", exec_strategy_str(t->strategy));
    cJSON_AddNumberToObject(obj, "token_budget", t->token_budget);
    cJSON_AddNumberToObject(obj, "tokens_used", t->tokens_used);
    if (t->error_reason[0]) cJSON_AddStringToObject(obj, "error_reason", t->error_reason);
    cJSON_AddNumberToObject(obj, "created_at", t->created_at);
    cJSON_AddNumberToObject(obj, "updated_at", t->updated_at);
    return obj;
}

cJSON *agent_to_json(Agent *a) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", a->id);
    cJSON_AddStringToObject(obj, "role", a->role);
    cJSON_AddStringToObject(obj, "state", agent_state_str(a->state));
    if (a->current_task_id[0]) cJSON_AddStringToObject(obj, "task_id", a->current_task_id);
    cJSON_AddNumberToObject(obj, "token_budget", a->token_budget);
    cJSON_AddNumberToObject(obj, "tokens_used", a->tokens_used);
    return obj;
}

cJSON *memory_to_json(Memory *m) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", m->id);
    cJSON_AddStringToObject(obj, "content", m->content);
    cJSON_AddStringToObject(obj, "kind", memory_kind_str(m->kind));
    cJSON_AddNumberToObject(obj, "importance", m->importance);
    cJSON_AddNumberToObject(obj, "access_count", m->access_count);
    cJSON_AddNumberToObject(obj, "ttl", m->ttl);
    cJSON_AddNumberToObject(obj, "created_at", m->created_at);
    cJSON_AddNumberToObject(obj, "last_accessed", m->last_accessed);
    return obj;
}
