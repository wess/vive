#include "vive.h"
#include <assert.h>
#include <unistd.h>

static const char *TEST_DB = "/tmp/vive_test.db";

static sqlite3 *setup(void) {
    unlink(TEST_DB);
    sqlite3 *db = NULL;
    assert(vive_db_open(TEST_DB, &db) == 0);
    return db;
}

static void test_task_crud(void) {
    sqlite3 *db = setup();

    Task t = {0};
    vive_gen_id(t.id);
    strncpy(t.name, "test-task", sizeof(t.name));
    t.state = TASK_PENDING;
    t.strategy = EXEC_SEQUENTIAL;
    t.token_budget = 10000;
    t.created_at = vive_now();
    t.updated_at = vive_now();
    assert(db_create_task(db, &t) == 0);

    assert(db_update_task_state(db, t.id, TASK_RUNNING, NULL) == 0);
    assert(db_update_task_tokens(db, t.id, 5000) == 0);
    assert(db_update_task_state(db, t.id, TASK_COMPLETED, NULL) == 0);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_agent_crud(void) {
    sqlite3 *db = setup();

    /* need a real task for FK */
    Task t = {0};
    vive_gen_id(t.id);
    strncpy(t.name, "agent-test-task", sizeof(t.name));
    t.state = TASK_RUNNING;
    t.strategy = EXEC_SEQUENTIAL;
    t.created_at = vive_now();
    t.updated_at = vive_now();
    db_create_task(db, &t);

    Agent a = {0};
    vive_gen_id(a.id);
    strncpy(a.role, "tester", sizeof(a.role));
    a.state = AGENT_IDLE;
    assert(db_register_agent(db, &a) == 0);

    assert(db_update_agent_state(db, a.id, AGENT_BUSY, t.id) == 0);
    assert(db_update_agent_state(db, a.id, AGENT_IDLE, NULL) == 0);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_memory_crud(void) {
    sqlite3 *db = setup();

    Memory m = {0};
    vive_gen_id(m.id);
    m.content = "test memory content";
    m.kind = MEM_STRUCTURED;
    m.importance = 1.0f;
    m.ttl = 0;
    m.created_at = vive_now();
    m.last_accessed = vive_now();
    strncpy(m.session_id, "sess1", 64);
    assert(db_store_memory(db, &m) == 0);

    assert(db_bump_memory_access(db, m.id) == 0);
    assert(db_forget_memory(db, m.id) == 0);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_memory_queries(void) {
    sqlite3 *db = setup();

    /* store some memories */
    for (int i = 0; i < 5; i++) {
        Memory m = {0};
        vive_gen_id(m.id);
        char buf[64];
        snprintf(buf, sizeof(buf), "decision %d", i);
        m.content = buf;
        m.kind = MEM_STRUCTURED;
        m.importance = 1.0f - (i * 0.1f);
        m.created_at = vive_now();
        m.last_accessed = vive_now();
        strncpy(m.session_id, "querysess", 64);
        db_store_memory(db, &m);
    }

    /* mem_query_top */
    cJSON *top = mem_query_top(db, "structured", 3);
    assert(top != NULL);
    assert(cJSON_GetArraySize(top) == 3);
    cJSON_Delete(top);

    /* mem_query_by_session */
    cJSON *sess = mem_query_by_session(db, "querysess");
    assert(sess != NULL);
    assert(cJSON_GetArraySize(sess) == 5);
    cJSON_Delete(sess);

    /* mem_query_top with NULL kind returns all */
    cJSON *all = mem_query_top(db, NULL, 10);
    assert(all != NULL);
    assert(cJSON_GetArraySize(all) >= 5);
    cJSON_Delete(all);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_session_and_stats(void) {
    sqlite3 *db = setup();

    assert(db_create_session(db, "s1", "test-client") == 0);
    assert(db_log_request(db, "s1", "initialize", 5, NULL) == 0);
    assert(db_log_request(db, "s1", "tools/call", 12, NULL) == 0);

    Stats st = {0};
    assert(db_query_stats(db, &st) == 0);
    assert(st.total_requests == 2);
    assert(st.connected_clients == 1);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_context_window(void) {
    sqlite3 *db = setup();

    /* need a task first */
    Task t = {0};
    vive_gen_id(t.id);
    strncpy(t.name, "ctx-test", sizeof(t.name));
    t.state = TASK_RUNNING;
    t.strategy = EXEC_SEQUENTIAL;
    t.token_budget = 40000;
    t.created_at = vive_now();
    t.updated_at = vive_now();
    db_create_task(db, &t);

    ContextWindow cw = {0};
    strncpy(cw.task_id, t.id, 16);
    cw.budget = 40000;
    assert(db_set_context_window(db, &cw) == 0);

    ContextWindow out = {0};
    assert(ctx_retrieve(db, t.id, &out) == 0);
    assert(out.budget == 40000);

    assert(ctx_budget(db, t.id, 50000) == 0);
    assert(ctx_retrieve(db, t.id, &out) == 0);
    assert(out.budget == 50000);

    vive_db_close(db);
    unlink(TEST_DB);
}

static void test_prune_expired(void) {
    sqlite3 *db = setup();

    Memory m = {0};
    vive_gen_id(m.id);
    m.content = "ephemeral";
    m.kind = MEM_SESSION;
    m.importance = 0.1f;
    m.ttl = 1; /* 1 second */
    m.created_at = vive_now() - 10; /* created 10s ago */
    m.last_accessed = m.created_at;
    db_store_memory(db, &m);

    sleep(1);
    assert(db_prune_expired(db) == 0);

    /* should be gone */
    cJSON *results = mem_query(db, "ephemeral", 10);
    assert(results != NULL);
    assert(cJSON_GetArraySize(results) == 0);
    cJSON_Delete(results);

    vive_db_close(db);
    unlink(TEST_DB);
}

int main(void) {
    test_task_crud();
    test_agent_crud();
    test_memory_crud();
    test_memory_queries();
    test_session_and_stats();
    test_context_window();
    test_prune_expired();
    printf("db: all tests passed\n");
    return 0;
}
