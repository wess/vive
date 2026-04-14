#include "vive.h"
#include <assert.h>

static void test_gen_id(void) {
    char id1[17], id2[17];
    vive_gen_id(id1);
    vive_gen_id(id2);
    assert(strlen(id1) == 16);
    assert(strlen(id2) == 16);
    /* ids should be hex chars */
    for (int i = 0; i < 16; i++) {
        assert((id1[i] >= '0' && id1[i] <= '9') || (id1[i] >= 'a' && id1[i] <= 'f'));
    }
}

static void test_task_state_roundtrip(void) {
    assert(task_state_parse("pending") == TASK_PENDING);
    assert(task_state_parse("running") == TASK_RUNNING);
    assert(task_state_parse("completed") == TASK_COMPLETED);
    assert(task_state_parse("failed") == TASK_FAILED);
    assert(task_state_parse("cancelled") == TASK_CANCELLED);
    assert(task_state_parse(NULL) == TASK_PENDING);
    assert(task_state_parse("bogus") == TASK_PENDING);
    assert(strcmp(task_state_str(TASK_RUNNING), "running") == 0);
}

static void test_exec_strategy_roundtrip(void) {
    assert(exec_strategy_parse("sequential") == EXEC_SEQUENTIAL);
    assert(exec_strategy_parse("parallel") == EXEC_PARALLEL);
    assert(exec_strategy_parse("supervised") == EXEC_SUPERVISED);
    assert(exec_strategy_parse("statemachine") == EXEC_STATEMACHINE);
    assert(exec_strategy_parse(NULL) == EXEC_SEQUENTIAL);
    assert(strcmp(exec_strategy_str(EXEC_PARALLEL), "parallel") == 0);
}

static void test_memory_kind_roundtrip(void) {
    assert(memory_kind_parse("semantic") == MEM_SEMANTIC);
    assert(memory_kind_parse("structured") == MEM_STRUCTURED);
    assert(memory_kind_parse("session") == MEM_SESSION);
    assert(memory_kind_parse(NULL) == MEM_STRUCTURED);
    assert(strcmp(memory_kind_str(MEM_SEMANTIC), "semantic") == 0);
}

static void test_agent_state(void) {
    assert(strcmp(agent_state_str(AGENT_IDLE), "idle") == 0);
    assert(strcmp(agent_state_str(AGENT_BUSY), "busy") == 0);
    assert(strcmp(agent_state_str(AGENT_ERROR), "error") == 0);
}

int main(void) {
    test_gen_id();
    test_task_state_roundtrip();
    test_exec_strategy_roundtrip();
    test_memory_kind_roundtrip();
    test_agent_state();
    printf("types: all tests passed\n");
    return 0;
}
