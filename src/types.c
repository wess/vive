#include "vive.h"

void vive_gen_id(char *buf) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned)(time(NULL) ^ clock()));
        seeded = 1;
    }
    for (int i = 0; i < 16; i++) {
        int v = rand() % 16;
        buf[i] = (v < 10) ? ('0' + v) : ('a' + v - 10);
    }
    buf[16] = '\0';
}

int vive_now(void) {
    return (int)time(NULL);
}

const char *task_state_str(TaskState s) {
    switch (s) {
        case TASK_PENDING:   return "pending";
        case TASK_RUNNING:   return "running";
        case TASK_COMPLETED: return "completed";
        case TASK_FAILED:    return "failed";
        case TASK_CANCELLED: return "cancelled";
    }
    return "pending";
}

TaskState task_state_parse(const char *s) {
    if (!s) return TASK_PENDING;
    if (strcmp(s, "running") == 0)   return TASK_RUNNING;
    if (strcmp(s, "completed") == 0) return TASK_COMPLETED;
    if (strcmp(s, "failed") == 0)    return TASK_FAILED;
    if (strcmp(s, "cancelled") == 0) return TASK_CANCELLED;
    return TASK_PENDING;
}

const char *exec_strategy_str(ExecStrategy s) {
    switch (s) {
        case EXEC_SEQUENTIAL:   return "sequential";
        case EXEC_PARALLEL:     return "parallel";
        case EXEC_SUPERVISED:   return "supervised";
        case EXEC_STATEMACHINE: return "statemachine";
    }
    return "sequential";
}

ExecStrategy exec_strategy_parse(const char *s) {
    if (!s) return EXEC_SEQUENTIAL;
    if (strcmp(s, "parallel") == 0)     return EXEC_PARALLEL;
    if (strcmp(s, "supervised") == 0)   return EXEC_SUPERVISED;
    if (strcmp(s, "statemachine") == 0) return EXEC_STATEMACHINE;
    return EXEC_SEQUENTIAL;
}

const char *memory_kind_str(MemoryKind k) {
    switch (k) {
        case MEM_SEMANTIC:   return "semantic";
        case MEM_STRUCTURED: return "structured";
        case MEM_SESSION:    return "session";
    }
    return "structured";
}

MemoryKind memory_kind_parse(const char *s) {
    if (!s) return MEM_STRUCTURED;
    if (strcmp(s, "semantic") == 0) return MEM_SEMANTIC;
    if (strcmp(s, "session") == 0)  return MEM_SESSION;
    return MEM_STRUCTURED;
}

const char *agent_state_str(AgentState s) {
    switch (s) {
        case AGENT_IDLE:  return "idle";
        case AGENT_BUSY:  return "busy";
        case AGENT_ERROR: return "error";
    }
    return "idle";
}
