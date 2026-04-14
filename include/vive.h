#ifndef VIVE_H
#define VIVE_H

#include <sqlite3.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

/* ── Types ─────────────────────────────────────────────── */

typedef enum {
    TASK_PENDING,
    TASK_RUNNING,
    TASK_COMPLETED,
    TASK_FAILED,
    TASK_CANCELLED
} TaskState;

typedef enum {
    EXEC_SEQUENTIAL,
    EXEC_PARALLEL,
    EXEC_SUPERVISED,
    EXEC_STATEMACHINE
} ExecStrategy;

typedef struct {
    char id[17];
    char name[256];
    TaskState state;
    ExecStrategy strategy;
    int token_budget;
    int tokens_used;
    char error_reason[256];
    int created_at;
    int updated_at;
} Task;

typedef struct {
    char id[17];
    char task_id[17];
    int step_index;
    TaskState state;
    char description[256];
    int started_at;
    int completed_at;
} TaskStep;

typedef enum {
    AGENT_IDLE,
    AGENT_BUSY,
    AGENT_ERROR
} AgentState;

typedef struct {
    char id[17];
    char role[64];
    AgentState state;
    char current_task_id[17];
    int token_budget;
    int tokens_used;
    char error_reason[256];
} Agent;

typedef enum {
    MEM_SEMANTIC,
    MEM_STRUCTURED,
    MEM_SESSION
} MemoryKind;

typedef struct {
    char id[17];
    char *content;
    MemoryKind kind;
    float importance;
    int access_count;
    int ttl;
    int created_at;
    int last_accessed;
    char session_id[65];
} Memory;

typedef struct {
    char task_id[17];
    int budget;
    int used;
    int compressed_size;
    int full_size;
} ContextWindow;

typedef struct {
    int tasks_pending;
    int tasks_running;
    int tasks_completed;
    int tasks_failed;
    int total_agents;
    int agents_busy;
    int agents_idle;
    int total_token_budget;
    int total_tokens_used;
    int total_compressed;
    int total_memories;
    int semantic_memories;
    int structured_memories;
    int session_memories;
    int total_requests;
    int total_errors;
    float avg_latency;
    int connected_clients;
} Stats;

/* ── Helpers ───────────────────────────────────────────── */

void        vive_gen_id(char *buf);
int         vive_now(void);
const char *task_state_str(TaskState s);
TaskState   task_state_parse(const char *s);
const char *exec_strategy_str(ExecStrategy s);
ExecStrategy exec_strategy_parse(const char *s);
const char *memory_kind_str(MemoryKind k);
MemoryKind  memory_kind_parse(const char *s);
const char *agent_state_str(AgentState s);

/* ── Schema & DB ───────────────────────────────────────── */

int  vive_schema_init(sqlite3 *db);
int  vive_db_open(const char *path, sqlite3 **out);
int  vive_db_open_readonly(const char *path, sqlite3 **out);
void vive_db_close(sqlite3 *db);

int  db_create_task(sqlite3 *db, Task *t);
int  db_update_task_state(sqlite3 *db, const char *id, TaskState state, const char *error);
int  db_update_task_tokens(sqlite3 *db, const char *id, int tokens);
int  db_register_agent(sqlite3 *db, Agent *a);
int  db_update_agent_state(sqlite3 *db, const char *id, AgentState state, const char *task_id);
int  db_store_memory(sqlite3 *db, Memory *m);
int  db_forget_memory(sqlite3 *db, const char *id);
int  db_bump_memory_access(sqlite3 *db, const char *id);
int  db_prune_expired(sqlite3 *db);
int  db_create_session(sqlite3 *db, const char *id, const char *client);
int  db_log_request(sqlite3 *db, const char *session_id, const char *method, int latency_ms, const char *error);
int  db_set_context_window(sqlite3 *db, ContextWindow *cw);
int  db_query_stats(sqlite3 *db, Stats *out);

/* ── Protocol ──────────────────────────────────────────── */

#define MCP_PARSE_ERROR      -32700
#define MCP_INVALID_REQUEST  -32600
#define MCP_METHOD_NOT_FOUND -32601
#define MCP_INVALID_PARAMS   -32602
#define MCP_INTERNAL_ERROR   -32603

char       *proto_response(cJSON *id, cJSON *result);
char       *proto_error(cJSON *id, int code, const char *msg);
char       *proto_init_response(cJSON *id);
char       *proto_tools_list(cJSON *id);
const char *proto_param_str(cJSON *params, const char *key);
int         proto_param_int(cJSON *params, const char *key, int def);
cJSON      *task_to_json(Task *t);
cJSON      *agent_to_json(Agent *a);
cJSON      *memory_to_json(Memory *m);

/* ── MCP Server ────────────────────────────────────────── */

void mcp_stdio_loop(sqlite3 *db);

/* ── Domain ────────────────────────────────────────────── */

int    ctx_budget(sqlite3 *db, const char *task_id, int limit);
int    ctx_inject(sqlite3 *db, const char *task_id, int tokens);
int    ctx_retrieve(sqlite3 *db, const char *task_id, ContextWindow *out);
char  *ctx_compress(sqlite3 *db, const char *task_id);

int    mem_store(sqlite3 *db, const char *content, const char *kind, int ttl, Memory *out);
cJSON *mem_query(sqlite3 *db, const char *text, int limit);
char  *mem_summarize(sqlite3 *db, cJSON *ids);
int    mem_prune(sqlite3 *db);

int    route_create(sqlite3 *db, const char *name, const char *strategy, int budget, Task *out);
int    route_run(sqlite3 *db, const char *task_id);
cJSON *route_status(sqlite3 *db, const char *task_id);
int    route_cancel(sqlite3 *db, const char *task_id);
cJSON *route_list(sqlite3 *db, const char *filter);
int    route_register_agent(sqlite3 *db, const char *role, int budget, Agent *out);
int    route_handoff(sqlite3 *db, const char *from, const char *to);
int    route_spawn_agent(sqlite3 *db, const char *role, const char *task_id, Agent *out);
cJSON *route_agent_status(sqlite3 *db, const char *agent_id);

/* ── New Queries ────────────────────────────────────────── */

cJSON *mem_query_top(sqlite3 *db, const char *kind, int limit);
cJSON *mem_query_by_session(sqlite3 *db, const char *session_id);

/* ── Init ──────────────────────────────────────────────── */

int vive_init(const char *project_dir, int global);
int vive_init_remove(const char *project_dir, int global);

/* ── Hooks ─────────────────────────────────────────────── */

int   hook_prompt(sqlite3 *db, const char *json);
int   hook_tool(sqlite3 *db, const char *json);
char *hook_session_start(sqlite3 *db, const char *json);
int   hook_stop(sqlite3 *db, const char *json);

/* ── Memories CLI ──────────────────────────────────────── */

int  mem_dedup(sqlite3 *db);
void mem_list(sqlite3 *db, const char *kind, int limit);
void mem_search(sqlite3 *db, const char *text, int limit);
void mem_forget_cli(sqlite3 *db, const char *id);
void mem_clear_all(sqlite3 *db, const char *kind);

/* ── TUI ───────────────────────────────────────────────── */

void tui_run(sqlite3 *db);
void print_status(sqlite3 *db);

#endif
