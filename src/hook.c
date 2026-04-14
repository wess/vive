#include "vive.h"
#include <regex.h>

#define CONTEXT_CAP 2000

static int match_decision(const char *text) {
    if (!text || strlen(text) < 40) return 0;
    static const char *patterns[] = {
        "(let's use|we'll use|we'll go with|going with|the stack is|using) .{20,}",
        "(we decided|the architecture is|the approach is|the plan is) .{20,}",
        "(don't|never|always|avoid|prefer) .{20,}",
        "(we should|we need to|we must|the convention is|the pattern is) .{20,}",
        "(switch to|migrate to|replace .+ with|deprecate) .{15,}",
        "(the database is|the api is|the frontend is|the backend is) .{15,}",
        NULL
    };
    regex_t re;
    for (int i = 0; patterns[i]; i++) {
        if (regcomp(&re, patterns[i], REG_EXTENDED | REG_ICASE | REG_NOSUB) == 0) {
            int match = regexec(&re, text, 0, NULL, 0) == 0;
            regfree(&re);
            if (match) return 1;
        }
    }
    return 0;
}

/* ── hook prompt ───────────────────────────────────────── */

int hook_prompt(sqlite3 *db, const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return 0;

    cJSON *hsi = cJSON_GetObjectItem(root, "hookSpecificInput");
    const char *prompt = NULL;
    if (hsi) {
        cJSON *p = cJSON_GetObjectItem(hsi, "prompt");
        if (p && cJSON_IsString(p)) prompt = p->valuestring;
    }
    const char *sess = NULL;
    cJSON *si = cJSON_GetObjectItem(root, "session_id");
    if (si && cJSON_IsString(si)) sess = si->valuestring;

    if (!prompt) { cJSON_Delete(root); return 0; }

    /* ephemeral session memory */
    Memory m = {0};
    vive_gen_id(m.id);
    m.content = strdup(prompt);
    m.kind = MEM_SESSION;
    m.importance = 0.1f;
    m.ttl = 86400;
    m.created_at = vive_now();
    m.last_accessed = vive_now();
    if (sess) strncpy(m.session_id, sess, 64);
    db_store_memory(db, &m);
    free(m.content);

    /* decision detection -> persistent */
    if (match_decision(prompt)) {
        Memory dm = {0};
        vive_gen_id(dm.id);
        dm.content = strdup(prompt);
        dm.kind = MEM_STRUCTURED;
        dm.importance = 1.0f;
        dm.ttl = 0;
        dm.created_at = vive_now();
        dm.last_accessed = vive_now();
        if (sess) strncpy(dm.session_id, sess, 64);
        db_store_memory(db, &dm);
        free(dm.content);
    }

    cJSON_Delete(root);
    return 0;
}

/* ── hook tool ─────────────────────────────────────────── */

int hook_tool(sqlite3 *db, const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return 0;

    const char *tool = NULL;
    cJSON *hsi = cJSON_GetObjectItem(root, "hookSpecificInput");
    if (hsi) {
        cJSON *tn = cJSON_GetObjectItem(hsi, "tool_name");
        if (tn && cJSON_IsString(tn)) tool = tn->valuestring;
    }
    const char *sess = NULL;
    cJSON *si = cJSON_GetObjectItem(root, "session_id");
    if (si && cJSON_IsString(si)) sess = si->valuestring;

    if (!tool) { cJSON_Delete(root); return 0; }

    /* skip noisy tools */
    if (strcmp(tool,"Read")==0 || strcmp(tool,"Glob")==0 || strcmp(tool,"Grep")==0) {
        cJSON_Delete(root); return 0;
    }

    cJSON *ti = NULL;
    if (hsi) ti = cJSON_GetObjectItem(hsi, "tool_input");

    /* file operations: persistent */
    if (strcmp(tool,"Write")==0 || strcmp(tool,"Edit")==0) {
        const char *fp = NULL;
        if (ti) {
            cJSON *p = cJSON_GetObjectItem(ti, "file_path");
            if (p && cJSON_IsString(p)) fp = p->valuestring;
        }
        if (fp) {
            char desc[512];
            snprintf(desc, sizeof(desc), "%s %s",
                strcmp(tool,"Write")==0 ? "created" : "modified", fp);
            Memory m = {0};
            vive_gen_id(m.id);
            m.content = desc;
            m.kind = MEM_STRUCTURED;
            m.importance = 0.7f;
            m.created_at = vive_now();
            m.last_accessed = vive_now();
            if (sess) strncpy(m.session_id, sess, 64);
            db_store_memory(db, &m);
        }
        cJSON_Delete(root);
        return 0;
    }

    /* bash commands */
    if (strcmp(tool,"Bash")==0 && ti) {
        cJSON *cmd = cJSON_GetObjectItem(ti, "command");
        if (cmd && cJSON_IsString(cmd)) {
            const char *c = cmd->valuestring;
            /* skip noisy commands */
            if (strncmp(c,"ls",2)==0 || strncmp(c,"cat",3)==0 ||
                strncmp(c,"head",4)==0 || strncmp(c,"tail",4)==0) {
                cJSON_Delete(root); return 0;
            }
            int is_setup = (strstr(c,"init")!=NULL || strstr(c,"build")!=NULL ||
                           strstr(c,"install")!=NULL || strstr(c,"make")!=NULL ||
                           strstr(c,"cargo")!=NULL || strstr(c,"goose")!=NULL);

            char desc[512];
            snprintf(desc, sizeof(desc), "ran: %.480s", c);
            Memory m = {0};
            vive_gen_id(m.id);
            m.content = desc;
            m.kind = is_setup ? MEM_STRUCTURED : MEM_SESSION;
            m.importance = is_setup ? 0.7f : 0.1f;
            m.ttl = is_setup ? 0 : 86400;
            m.created_at = vive_now();
            m.last_accessed = vive_now();
            if (sess) strncpy(m.session_id, sess, 64);
            db_store_memory(db, &m);
        }
        cJSON_Delete(root);
        return 0;
    }

    /* all other tools: ephemeral */
    char desc[256];
    snprintf(desc, sizeof(desc), "used tool: %s", tool);
    Memory m = {0};
    vive_gen_id(m.id);
    m.content = desc;
    m.kind = MEM_SESSION;
    m.importance = 0.1f;
    m.ttl = 86400;
    m.created_at = vive_now();
    m.last_accessed = vive_now();
    if (sess) strncpy(m.session_id, sess, 64);
    db_store_memory(db, &m);

    /* auto-compression: check running tasks at 80% budget */
    sqlite3_stmt *ts = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT t.id, t.token_budget, t.tokens_used FROM tasks t"
        " WHERE t.state='running' AND t.token_budget > 0"
        " AND CAST(t.tokens_used AS REAL) / t.token_budget >= 0.8",
        -1, &ts, NULL) == SQLITE_OK) {
        while (sqlite3_step(ts) == SQLITE_ROW) {
            const char *tid = (const char *)sqlite3_column_text(ts, 0);
            if (!tid) continue;
            /* compress: extract first line of each completed step */
            char *compressed = ctx_compress(db, tid);
            if (compressed && compressed[0]) {
                /* store as medium-lived memory */
                Memory cm = {0};
                vive_gen_id(cm.id);
                cm.content = compressed;
                cm.kind = MEM_SEMANTIC;
                cm.importance = 0.5f;
                cm.ttl = 604800;
                cm.created_at = vive_now();
                cm.last_accessed = vive_now();
                if (sess) strncpy(cm.session_id, sess, 64);
                db_store_memory(db, &cm);

                /* update context window compressed_size */
                int clen = (int)strlen(compressed);
                ContextWindow cw = {0};
                if (ctx_retrieve(db, tid, &cw) == 0) {
                    cw.compressed_size = clen / 4; /* chars/4 ~ tokens */
                    db_set_context_window(db, &cw);
                }
            }
            free(compressed);
        }
        sqlite3_finalize(ts);
    }

    cJSON_Delete(root);
    return 0;
}

/* ── hook session-start ────────────────────────────────── */

char *hook_session_start(sqlite3 *db, const char *json) {
    /* 1. top persistent memories */
    cJSON *top = mem_query_top(db, "structured", 10);

    /* 2. last session summary */
    sqlite3_stmt *s = NULL;
    char last_summary[512] = {0};
    if (sqlite3_prepare_v2(db,
        "SELECT content FROM memories WHERE kind='semantic' ORDER BY created_at DESC LIMIT 1",
        -1, &s, NULL) == SQLITE_OK) {
        if (sqlite3_step(s) == SQLITE_ROW) {
            const char *c = (const char *)sqlite3_column_text(s, 0);
            if (c) strncpy(last_summary, c, 511);
        }
        sqlite3_finalize(s);
    }

    /* 3. active tasks */
    int running = 0, pending = 0;
    if (sqlite3_prepare_v2(db,
        "SELECT state, COUNT(*) FROM tasks WHERE state IN ('pending','running') GROUP BY state",
        -1, &s, NULL) == SQLITE_OK) {
        while (sqlite3_step(s) == SQLITE_ROW) {
            const char *st = (const char *)sqlite3_column_text(s, 0);
            int n = sqlite3_column_int(s, 1);
            if (strcmp(st, "running") == 0) running = n;
            else pending = n;
        }
        sqlite3_finalize(s);
    }

    /* 4. build context string */
    char *ctx = malloc(CONTEXT_CAP + 256);
    int off = 0;
    off += snprintf(ctx + off, CONTEXT_CAP - off, "[Vive Context]\n");

    if (last_summary[0])
        off += snprintf(ctx + off, CONTEXT_CAP - off, "Last session: %s\n", last_summary);
    if (running || pending)
        off += snprintf(ctx + off, CONTEXT_CAP - off, "Active tasks: %d running, %d pending\n", running, pending);

    if (top) {
        int n = cJSON_GetArraySize(top);
        if (n > 0) {
            off += snprintf(ctx + off, CONTEXT_CAP - off, "Decisions:\n");
            for (int i = 0; i < n && off < CONTEXT_CAP; i++) {
                cJSON *item = cJSON_GetArrayItem(top, i);
                cJSON *content = cJSON_GetObjectItem(item, "content");
                if (content && cJSON_IsString(content)) {
                    char line[128];
                    strncpy(line, content->valuestring, 120);
                    line[120] = '\0';
                    off += snprintf(ctx + off, CONTEXT_CAP - off, "- %s\n", line);
                }
            }
        }
        cJSON_Delete(top);
    }

    /* 5. update CLAUDE.md */
    cJSON *root = cJSON_Parse(json);
    const char *cwd = ".";
    if (root) {
        cJSON *c = cJSON_GetObjectItem(root, "cwd");
        if (c && cJSON_IsString(c)) cwd = c->valuestring;
    }

    char claude_path[512];
    snprintf(claude_path, sizeof(claude_path), "%s/CLAUDE.md", cwd);

    /* read existing */
    FILE *f = fopen(claude_path, "r");
    char *claude_content = NULL;
    long claude_len = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        claude_len = ftell(f);
        fseek(f, 0, SEEK_SET);
        claude_content = malloc(claude_len + 1);
        fread(claude_content, 1, claude_len, f);
        claude_content[claude_len] = '\0';
        fclose(f);
    }

    /* build managed section */
    cJSON *top2 = mem_query_top(db, "structured", 5);
    char managed[2048];
    int moff = 0;
    moff += snprintf(managed + moff, sizeof(managed) - moff,
        "<!-- vive:start -->\n## Project Context (managed by Vive)\n\n");
    if (top2) {
        int n2 = cJSON_GetArraySize(top2);
        for (int i = 0; i < n2; i++) {
            cJSON *item = cJSON_GetArrayItem(top2, i);
            cJSON *content = cJSON_GetObjectItem(item, "content");
            if (content && cJSON_IsString(content))
                moff += snprintf(managed + moff, sizeof(managed) - moff, "- %s\n", content->valuestring);
        }
        cJSON_Delete(top2);
    }
    moff += snprintf(managed + moff, sizeof(managed) - moff, "<!-- vive:end -->");

    /* write CLAUDE.md */
    f = fopen(claude_path, "w");
    if (f) {
        if (claude_content) {
            char *start = strstr(claude_content, "<!-- vive:start -->");
            char *end = strstr(claude_content, "<!-- vive:end -->");
            if (start && end) {
                fwrite(claude_content, 1, start - claude_content, f);
                fprintf(f, "%s", managed);
                char *after = end + strlen("<!-- vive:end -->");
                fprintf(f, "%s", after);
            } else {
                fprintf(f, "%s\n\n%s\n", claude_content, managed);
            }
        } else {
            fprintf(f, "%s\n", managed);
        }
        fclose(f);
    }
    free(claude_content);

    /* 6. return hook output */
    cJSON *output = cJSON_CreateObject();
    cJSON *hso = cJSON_CreateObject();
    cJSON_AddStringToObject(hso, "hookEventName", "SessionStart");
    cJSON_AddStringToObject(hso, "additionalContext", ctx);
    cJSON_AddItemToObject(output, "hookSpecificOutput", hso);

    char *result = cJSON_PrintUnformatted(output);
    cJSON_Delete(output);
    if (root) cJSON_Delete(root);
    free(ctx);
    return result;
}

/* ── hook stop ─────────────────────────────────────────── */

int hook_stop(sqlite3 *db, const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return 0;

    const char *sess = NULL;
    cJSON *si = cJSON_GetObjectItem(root, "session_id");
    if (si && cJSON_IsString(si)) sess = si->valuestring;

    if (!sess) { cJSON_Delete(root); return 0; }

    cJSON *mems = mem_query_by_session(db, sess);
    if (!mems) { cJSON_Delete(root); return 0; }

    int n = cJSON_GetArraySize(mems);
    if (n == 0) { cJSON_Delete(mems); cJSON_Delete(root); return 0; }

    /* build summary */
    char summary[2048] = {0};
    int soff = 0;
    int count = (n > 10) ? 10 : n;
    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(mems, i);
        cJSON *content = cJSON_GetObjectItem(item, "content");
        if (content && cJSON_IsString(content)) {
            char chunk[84];
            strncpy(chunk, content->valuestring, 80);
            chunk[80] = '\0';
            soff += snprintf(summary + soff, sizeof(summary) - soff, "%s%s",
                i > 0 ? "; " : "", chunk);
        }
    }
    cJSON_Delete(mems);

    /* store as medium-lived */
    if (summary[0]) {
        Memory m = {0};
        vive_gen_id(m.id);
        m.content = summary;
        m.kind = MEM_SEMANTIC;
        m.importance = 0.5f;
        m.ttl = 604800;
        m.created_at = vive_now();
        m.last_accessed = vive_now();
        strncpy(m.session_id, sess, 64);
        db_store_memory(db, &m);
    }

    db_prune_expired(db);
    mem_dedup(db);
    cJSON_Delete(root);
    return 0;
}
