#include "vive.h"
#include <assert.h>

static void test_proto_response(void) {
    cJSON *id = cJSON_CreateNumber(1);
    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "status", "ok");

    char *resp = proto_response(id, result);
    assert(resp != NULL);
    assert(strstr(resp, "\"jsonrpc\":\"2.0\"") != NULL);
    assert(strstr(resp, "\"status\":\"ok\"") != NULL);
    free(resp);
    cJSON_Delete(id);
}

static void test_proto_error(void) {
    cJSON *id = cJSON_CreateNumber(1);
    char *resp = proto_error(id, MCP_INVALID_PARAMS, "missing field");
    assert(resp != NULL);
    assert(strstr(resp, "-32602") != NULL);
    assert(strstr(resp, "missing field") != NULL);
    free(resp);
    cJSON_Delete(id);
}

static void test_proto_init(void) {
    cJSON *id = cJSON_CreateNumber(1);
    char *resp = proto_init_response(id);
    assert(resp != NULL);
    assert(strstr(resp, "vive") != NULL);
    assert(strstr(resp, "0.1.0") != NULL);
    assert(strstr(resp, "2024-11-05") != NULL);
    free(resp);
    cJSON_Delete(id);
}

static void test_proto_tools_list(void) {
    cJSON *id = cJSON_CreateNumber(1);
    char *resp = proto_tools_list(id);
    assert(resp != NULL);

    cJSON *root = cJSON_Parse(resp);
    assert(root != NULL);
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *tools = cJSON_GetObjectItem(result, "tools");
    assert(tools != NULL);
    assert(cJSON_GetArraySize(tools) == 17);

    /* verify first tool has proper schema */
    cJSON *first = cJSON_GetArrayItem(tools, 0);
    cJSON *schema = cJSON_GetObjectItem(first, "inputSchema");
    assert(schema != NULL);
    cJSON *props = cJSON_GetObjectItem(schema, "properties");
    assert(props != NULL);
    cJSON *req = cJSON_GetObjectItem(schema, "required");
    assert(req != NULL);

    cJSON_Delete(root);
    free(resp);
    cJSON_Delete(id);
}

static void test_proto_param_helpers(void) {
    cJSON *params = cJSON_CreateObject();
    cJSON_AddStringToObject(params, "name", "test");
    cJSON_AddNumberToObject(params, "limit", 42);

    assert(strcmp(proto_param_str(params, "name"), "test") == 0);
    assert(proto_param_str(params, "missing") == NULL);
    assert(proto_param_str(NULL, "name") == NULL);

    assert(proto_param_int(params, "limit", 0) == 42);
    assert(proto_param_int(params, "missing", 99) == 99);
    assert(proto_param_int(NULL, "limit", 99) == 99);

    cJSON_Delete(params);
}

static void test_json_serializers(void) {
    Task t = {0};
    strncpy(t.id, "abc123", 16);
    strncpy(t.name, "my-task", 255);
    t.state = TASK_RUNNING;
    t.strategy = EXEC_PARALLEL;
    t.token_budget = 10000;
    t.tokens_used = 3000;
    t.created_at = 1000;
    t.updated_at = 2000;

    cJSON *j = task_to_json(&t);
    assert(j != NULL);
    cJSON *name = cJSON_GetObjectItem(j, "name");
    assert(strcmp(name->valuestring, "my-task") == 0);
    cJSON *state = cJSON_GetObjectItem(j, "state");
    assert(strcmp(state->valuestring, "running") == 0);
    cJSON_Delete(j);

    Agent a = {0};
    strncpy(a.id, "agent1", 16);
    strncpy(a.role, "reviewer", 63);
    a.state = AGENT_BUSY;
    strncpy(a.current_task_id, "abc123", 16);

    j = agent_to_json(&a);
    assert(j != NULL);
    cJSON *role = cJSON_GetObjectItem(j, "role");
    assert(strcmp(role->valuestring, "reviewer") == 0);
    cJSON *tid = cJSON_GetObjectItem(j, "task_id");
    assert(strcmp(tid->valuestring, "abc123") == 0);
    cJSON_Delete(j);

    Memory m = {0};
    strncpy(m.id, "mem1", 16);
    m.content = "test content";
    m.kind = MEM_SEMANTIC;
    m.importance = 0.75f;

    j = memory_to_json(&m);
    assert(j != NULL);
    cJSON *kind = cJSON_GetObjectItem(j, "kind");
    assert(strcmp(kind->valuestring, "semantic") == 0);
    cJSON_Delete(j);
}

int main(void) {
    test_proto_response();
    test_proto_error();
    test_proto_init();
    test_proto_tools_list();
    test_proto_param_helpers();
    test_json_serializers();
    printf("protocol: all tests passed\n");
    return 0;
}
