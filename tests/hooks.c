#include "vive.h"
#include <assert.h>
#include <unistd.h>

static const char *TEST_DB = "/tmp/vive_hook_test.db";

static sqlite3 *setup(void) {
    unlink(TEST_DB);
    sqlite3 *db = NULL;
    assert(vive_db_open(TEST_DB, &db) == 0);
    return db;
}

static void test_hook_prompt_decision(void) {
    sqlite3 *db = setup();

    const char *json =
        "{\"hookSpecificInput\":{\"prompt\":\"we decided to use PostgreSQL for the main database and Redis for caching\"},"
        "\"session_id\":\"test-sess-1\",\"cwd\":\"/tmp\"}";
    assert(hook_prompt(db, json) == 0);

    /* should have 2 memories: 1 session + 1 structured decision */
    Stats st = {0};
    db_query_stats(db, &st);
    assert(st.structured_memories == 1);
    assert(st.session_memories == 1);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_hook_prompt_no_decision(void) {
    sqlite3 *db = setup();

    const char *json =
        "{\"hookSpecificInput\":{\"prompt\":\"what files are in this directory?\"},"
        "\"session_id\":\"test-sess-2\",\"cwd\":\"/tmp\"}";
    assert(hook_prompt(db, json) == 0);

    Stats st = {0};
    db_query_stats(db, &st);
    assert(st.structured_memories == 0);
    assert(st.session_memories == 1);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_hook_tool_write(void) {
    sqlite3 *db = setup();

    const char *json =
        "{\"hookSpecificInput\":{\"tool_name\":\"Write\",\"tool_input\":{\"file_path\":\"src/auth.c\"}},"
        "\"session_id\":\"test-sess-3\",\"cwd\":\"/tmp\"}";
    assert(hook_tool(db, json) == 0);

    Stats st = {0};
    db_query_stats(db, &st);
    assert(st.structured_memories == 1);

    /* verify content */
    cJSON *top = mem_query_top(db, "structured", 1);
    assert(top != NULL);
    assert(cJSON_GetArraySize(top) == 1);
    cJSON *item = cJSON_GetArrayItem(top, 0);
    cJSON *content = cJSON_GetObjectItem(item, "content");
    assert(content != NULL);
    assert(strstr(content->valuestring, "created src/auth.c") != NULL);
    cJSON_Delete(top);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_hook_tool_skip_read(void) {
    sqlite3 *db = setup();

    const char *json =
        "{\"hookSpecificInput\":{\"tool_name\":\"Read\",\"tool_input\":{\"file_path\":\"src/main.c\"}},"
        "\"session_id\":\"test-sess-4\",\"cwd\":\"/tmp\"}";
    assert(hook_tool(db, json) == 0);

    Stats st = {0};
    db_query_stats(db, &st);
    assert(st.total_memories == 0);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_hook_session_start(void) {
    sqlite3 *db = setup();

    /* store a decision first */
    Memory m = {0};
    vive_gen_id(m.id);
    m.content = "using Go for the API";
    m.kind = MEM_STRUCTURED;
    m.importance = 1.0f;
    m.created_at = vive_now();
    m.last_accessed = vive_now();
    db_store_memory(db, &m);

    const char *json = "{\"session_id\":\"start-sess\",\"cwd\":\"/tmp\"}";
    char *out = hook_session_start(db, json);
    assert(out != NULL);

    /* parse and verify */
    cJSON *root = cJSON_Parse(out);
    assert(root != NULL);
    cJSON *hso = cJSON_GetObjectItem(root, "hookSpecificOutput");
    assert(hso != NULL);
    cJSON *ctx = cJSON_GetObjectItem(hso, "additionalContext");
    assert(ctx != NULL);
    assert(strstr(ctx->valuestring, "Vive Context") != NULL);
    assert(strstr(ctx->valuestring, "using Go for the API") != NULL);
    cJSON_Delete(root);
    free(out);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_hook_stop(void) {
    sqlite3 *db = setup();

    /* store some session memories */
    for (int i = 0; i < 3; i++) {
        Memory m = {0};
        vive_gen_id(m.id);
        char buf[64];
        snprintf(buf, sizeof(buf), "session work item %d", i);
        m.content = buf;
        m.kind = MEM_SESSION;
        m.importance = 0.1f;
        m.ttl = 86400;
        m.created_at = vive_now();
        m.last_accessed = vive_now();
        strncpy(m.session_id, "stop-sess", 64);
        db_store_memory(db, &m);
    }

    const char *json = "{\"session_id\":\"stop-sess\"}";
    assert(hook_stop(db, json) == 0);

    /* should have created a semantic summary */
    Stats st = {0};
    db_query_stats(db, &st);
    assert(st.semantic_memories == 1);

    vive_db_close(db);
    unlink(TEST_DB);
}

int main(void) {
    test_hook_prompt_decision();
    test_hook_prompt_no_decision();
    test_hook_tool_write();
    test_hook_tool_skip_read();
    test_hook_session_start();
    test_hook_stop();
    printf("hooks: all tests passed\n");
    return 0;
}
