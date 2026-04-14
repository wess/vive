#include "vive.h"
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static void get_vive_bin_path(char *out, size_t sz) {
#ifdef __APPLE__
    uint32_t len = (uint32_t)sz;
    if (_NSGetExecutablePath(out, &len) == 0) {
        char resolved[1024];
        if (realpath(out, resolved)) {
            strncpy(out, resolved, sz - 1);
            out[sz - 1] = '\0';
        }
        return;
    }
#endif
    /* fallback: try /proc/self/exe */
    ssize_t n = readlink("/proc/self/exe", out, sz - 1);
    if (n > 0) { out[n] = '\0'; return; }
    strncpy(out, "vive", sz - 1);
}

static int write_settings(const char *dir, const char *vive_path) {
    char settings_path[512];
    snprintf(settings_path, sizeof(settings_path), "%s/settings.json", dir);

    /* read existing */
    cJSON *root = NULL;
    FILE *f = fopen(settings_path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = malloc(len + 1);
        fread(buf, 1, len, f);
        buf[len] = '\0';
        fclose(f);
        root = cJSON_Parse(buf);
        free(buf);
    }
    if (!root) root = cJSON_CreateObject();

    /* upsert mcpServers.vive */
    cJSON *servers = cJSON_GetObjectItem(root, "mcpServers");
    if (!servers) { servers = cJSON_CreateObject(); cJSON_AddItemToObject(root, "mcpServers", servers); }
    cJSON_DeleteItemFromObject(servers, "vive");
    cJSON *vive_srv = cJSON_CreateObject();
    cJSON_AddStringToObject(vive_srv, "command", vive_path);
    cJSON *args = cJSON_CreateArray();
    cJSON_AddItemToArray(args, cJSON_CreateString("--daemon"));
    cJSON_AddItemToObject(vive_srv, "args", args);
    cJSON_AddItemToObject(servers, "vive", vive_srv);

    /* upsert hooks */
    cJSON *hooks = cJSON_GetObjectItem(root, "hooks");
    if (!hooks) { hooks = cJSON_CreateObject(); cJSON_AddItemToObject(root, "hooks", hooks); }

    struct { const char *event; const char *suffix; int async; } defs[] = {
        { "UserPromptSubmit", "hook prompt",       1 },
        { "PostToolUse",      "hook tool",          1 },
        { "SessionStart",     "hook session-start", 0 },
        { "Stop",             "hook stop",          1 },
    };

    for (int i = 0; i < 4; i++) {
        char cmd[1024];
        snprintf(cmd, sizeof(cmd), "%s %s", vive_path, defs[i].suffix);

        /* check if already exists */
        cJSON *arr = cJSON_GetObjectItem(hooks, defs[i].event);
        int exists = 0;
        if (arr && cJSON_IsArray(arr)) {
            int n = cJSON_GetArraySize(arr);
            for (int j = 0; j < n && !exists; j++) {
                cJSON *entry = cJSON_GetArrayItem(arr, j);
                cJSON *harr = cJSON_GetObjectItem(entry, "hooks");
                if (harr && cJSON_IsArray(harr)) {
                    int hn = cJSON_GetArraySize(harr);
                    for (int k = 0; k < hn; k++) {
                        cJSON *h = cJSON_GetArrayItem(harr, k);
                        cJSON *c = cJSON_GetObjectItem(h, "command");
                        if (c && cJSON_IsString(c) && strstr(c->valuestring, "vive hook")) exists = 1;
                    }
                }
            }
        }

        if (!exists) {
            if (!arr) { arr = cJSON_CreateArray(); cJSON_AddItemToObject(hooks, defs[i].event, arr); }
            cJSON *entry = cJSON_CreateObject();
            cJSON_AddStringToObject(entry, "matcher", "*");
            cJSON *harr = cJSON_CreateArray();
            cJSON *hook = cJSON_CreateObject();
            cJSON_AddStringToObject(hook, "type", "command");
            cJSON_AddStringToObject(hook, "command", cmd);
            if (defs[i].async) cJSON_AddBoolToObject(hook, "async", 1);
            cJSON_AddItemToArray(harr, hook);
            cJSON_AddItemToObject(entry, "hooks", harr);
            cJSON_AddItemToArray(arr, entry);
        }
    }

    /* write */
    char *out = cJSON_Print(root);
    f = fopen(settings_path, "w");
    if (!f) { cJSON_Delete(root); free(out); return -1; }
    fprintf(f, "%s\n", out);
    fclose(f);
    free(out);
    cJSON_Delete(root);
    return 0;
}

int vive_init(const char *project_dir, int global) {
    char vive_path[1024];
    get_vive_bin_path(vive_path, sizeof(vive_path));

    char claude_dir[512];
    if (global) {
        const char *home = getenv("HOME");
        if (!home) { fprintf(stderr, "error: HOME not set\n"); return -1; }
        snprintf(claude_dir, sizeof(claude_dir), "%s/.claude", home);
    } else {
        snprintf(claude_dir, sizeof(claude_dir), "%s/.claude", project_dir);
    }
    mkdir(claude_dir, 0755);

    printf("vive: configuring %s integration...\n", global ? "global" : "project");

    /* settings */
    if (write_settings(claude_dir, vive_path) != 0) {
        fprintf(stderr, "error: failed to write settings\n");
        return -1;
    }
    printf("  + MCP server registered\n");
    printf("  + hooks configured\n");

    /* slash command */
    char cmd_dir[512];
    snprintf(cmd_dir, sizeof(cmd_dir), "%s/commands", claude_dir);
    mkdir(cmd_dir, 0755);

    char cmd_path[512];
    snprintf(cmd_path, sizeof(cmd_path), "%s/vive.md", cmd_dir);
    FILE *f = fopen(cmd_path, "w");
    if (f) {
        fprintf(f, "# /vive\n\n");
        fprintf(f, "Interact with the Vive orchestration server.\n\n");
        fprintf(f, "Use Vive's MCP tools to manage tasks, agents, memory, and context.\n\n");
        fprintf(f, "## Argument: $ARGUMENTS\n\n");
        fprintf(f, "Interpret as a natural language request and use the appropriate Vive MCP tools.\n");
        fclose(f);
        printf("  + /vive command installed\n");
    }

    /* CLAUDE.md managed section (project mode only) */
    if (!global) {
        char claude_md[512];
        snprintf(claude_md, sizeof(claude_md), "%s/CLAUDE.md", project_dir);
        f = fopen(claude_md, "r");
        int has_section = 0;
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *buf = malloc(len + 1);
            fread(buf, 1, len, f);
            buf[len] = '\0';
            fclose(f);
            has_section = (strstr(buf, "<!-- vive:start -->") != NULL);
            if (!has_section) {
                f = fopen(claude_md, "a");
                if (f) {
                    fprintf(f, "\n\n<!-- vive:start -->\n## Project Context (managed by Vive)\n\n<!-- vive:end -->\n");
                    fclose(f);
                }
            }
            free(buf);
        } else {
            f = fopen(claude_md, "w");
            if (f) {
                fprintf(f, "<!-- vive:start -->\n## Project Context (managed by Vive)\n\n<!-- vive:end -->\n");
                fclose(f);
            }
        }
        printf("  + CLAUDE.md initialized\n");
    }

    printf("\nvive: ready! restart Claude Code to activate.\n");
    return 0;
}

static int remove_settings(const char *dir) {
    char settings_path[512];
    snprintf(settings_path, sizeof(settings_path), "%s/settings.json", dir);

    FILE *f = fopen(settings_path, "r");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return 0;

    /* remove mcpServers.vive */
    cJSON *servers = cJSON_GetObjectItem(root, "mcpServers");
    if (servers) cJSON_DeleteItemFromObject(servers, "vive");

    /* remove vive hooks from each event */
    cJSON *hooks = cJSON_GetObjectItem(root, "hooks");
    if (hooks) {
        const char *events[] = {"UserPromptSubmit","PostToolUse","SessionStart","Stop",NULL};
        for (int i = 0; events[i]; i++) {
            cJSON *arr = cJSON_GetObjectItem(hooks, events[i]);
            if (!arr || !cJSON_IsArray(arr)) continue;
            for (int j = cJSON_GetArraySize(arr) - 1; j >= 0; j--) {
                cJSON *entry = cJSON_GetArrayItem(arr, j);
                cJSON *harr = cJSON_GetObjectItem(entry, "hooks");
                if (!harr) continue;
                for (int k = 0; k < cJSON_GetArraySize(harr); k++) {
                    cJSON *h = cJSON_GetArrayItem(harr, k);
                    cJSON *c = cJSON_GetObjectItem(h, "command");
                    if (c && cJSON_IsString(c) && strstr(c->valuestring, "vive hook")) {
                        cJSON_DeleteItemFromArray(arr, j);
                        break;
                    }
                }
            }
            if (cJSON_GetArraySize(arr) == 0)
                cJSON_DeleteItemFromObject(hooks, events[i]);
        }
    }

    char *out = cJSON_Print(root);
    f = fopen(settings_path, "w");
    if (f) { fprintf(f, "%s\n", out); fclose(f); }
    free(out);
    cJSON_Delete(root);
    return 0;
}

int vive_init_remove(const char *project_dir, int global) {
    char claude_dir[512];
    if (global) {
        const char *home = getenv("HOME");
        if (!home) { fprintf(stderr, "error: HOME not set\n"); return -1; }
        snprintf(claude_dir, sizeof(claude_dir), "%s/.claude", home);
    } else {
        snprintf(claude_dir, sizeof(claude_dir), "%s/.claude", project_dir);
    }

    printf("vive: removing %s integration...\n", global ? "global" : "project");

    remove_settings(claude_dir);
    printf("  - MCP server removed\n");
    printf("  - hooks removed\n");

    /* remove slash command */
    char cmd_path[512];
    snprintf(cmd_path, sizeof(cmd_path), "%s/commands/vive.md", claude_dir);
    unlink(cmd_path);
    printf("  - /vive command removed\n");

    /* remove CLAUDE.md managed section (project only) */
    if (!global) {
        char claude_md[512];
        snprintf(claude_md, sizeof(claude_md), "%s/CLAUDE.md", project_dir);
        FILE *f = fopen(claude_md, "r");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *buf = malloc(len + 1);
            fread(buf, 1, len, f);
            buf[len] = '\0';
            fclose(f);

            char *start = strstr(buf, "<!-- vive:start -->");
            char *end = strstr(buf, "<!-- vive:end -->");
            if (start && end) {
                f = fopen(claude_md, "w");
                if (f) {
                    /* write content before the section */
                    fwrite(buf, 1, start - buf, f);
                    /* skip section + any trailing newlines */
                    char *after = end + strlen("<!-- vive:end -->");
                    while (*after == '\n') after++;
                    if (*after) fprintf(f, "%s", after);
                    fclose(f);
                }
            }
            free(buf);
        }
        printf("  - CLAUDE.md section removed\n");
    }

    printf("\nvive: removed. restart Claude Code to deactivate.\n");
    return 0;
}
