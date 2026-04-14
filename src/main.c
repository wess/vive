#include "vive.h"
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

static sqlite3 *g_db = NULL;

static const char *get_global_db_path(void) {
    const char *home = getenv("HOME");
    if (!home) return "vive.db";
    static char path[512];
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.local/share/vive", home);
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/vive.db", dir);
    return path;
}

static const char *get_db_path(void) {
    const char *env = getenv("VIVE_DB_PATH");
    if (env) return env;

    /* try project-scoped: .vive/vive.db in cwd */
    static char path[512];
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        char vive_dir[512];
        snprintf(vive_dir, sizeof(vive_dir), "%s/.vive", cwd);
        struct stat st;
        if (stat(vive_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            snprintf(path, sizeof(path), "%s/vive.db", vive_dir);
            return path;
        }
        /* check if .claude/settings.json exists (project root marker) */
        char claude_settings[512];
        snprintf(claude_settings, sizeof(claude_settings), "%s/.claude/settings.json", cwd);
        if (stat(claude_settings, &st) == 0) {
            mkdir(vive_dir, 0755);
            snprintf(path, sizeof(path), "%s/vive.db", vive_dir);
            return path;
        }
    }

    return get_global_db_path();
}

static void write_pid(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char dir[512], path[512];
    snprintf(dir, sizeof(dir), "%s/.local/share/vive", home);
    mkdir(dir, 0755);
    snprintf(path, sizeof(path), "%s/vive.pid", dir);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "%d", getpid()); fclose(f); }
}

static void remove_pid(void) {
    const char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.local/share/vive/vive.pid", home);
    unlink(path);
}

static void on_signal(int sig) {
    (void)sig;
    if (g_db) vive_db_close(g_db);
    remove_pid();
    _exit(0);
}

/* MCP server runs on a background thread reading stdin */
static void *mcp_thread(void *arg) {
    sqlite3 *db = (sqlite3 *)arg;
    mcp_stdio_loop(db);
    return NULL;
}

static void usage(void) {
    printf("vive - smart harness for AI agent orchestration\n\n");
    printf("usage:\n");
    printf("  vive              start server with live TUI dashboard\n");
    printf("  vive --daemon     start server in background (no TUI)\n");
    printf("  vive status       print status snapshot and exit\n");
    printf("  vive init           set up Claude Code integration (hooks + MCP)\n");
    printf("  vive init --global  set up for all projects\n");
    printf("  vive init --remove  remove Vive integration\n");
    printf("  vive memories       list memories (--kind, --search, --forget, --clear)\n");
    printf("  vive hook <event>   process a Claude Code hook event\n");
    printf("  vive help         show this message\n\n");
    printf("The server listens for MCP connections on stdio.\n");
    printf("Press q to quit the TUI (stops the server).\n\n");
    printf("environment:\n");
    printf("  VIVE_DB_PATH    path to database (default: ~/.local/share/vive/vive.db)\n");
}

int main(int argc, char *argv[]) {
    /* handle subcommands */
    if (argc >= 2) {
        if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage();
            return 0;
        }
        if (strcmp(argv[1], "status") == 0) {
            const char *db_path = get_db_path();
            sqlite3 *db = NULL;
            if (vive_db_open_readonly(db_path, &db) != 0) {
                fprintf(stderr, "error: cannot open database at %s\nis vive running?\n", db_path);
                return 1;
            }
            print_status(db);
            vive_db_close(db);
            return 0;
        }
        if (strcmp(argv[1], "memories") == 0) {
            const char *db_path = get_db_path();
            sqlite3 *db = NULL;
            if (vive_db_open(db_path, &db) != 0) {
                fprintf(stderr, "error: cannot open database\n");
                return 1;
            }
            /* parse flags */
            const char *kind = NULL, *search = NULL, *forget_id = NULL;
            int do_clear = 0, do_dedup = 0, limit = 20;
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "--kind") == 0 && i+1 < argc) kind = argv[++i];
                else if (strcmp(argv[i], "--search") == 0 && i+1 < argc) search = argv[++i];
                else if (strcmp(argv[i], "--forget") == 0 && i+1 < argc) forget_id = argv[++i];
                else if (strcmp(argv[i], "--clear") == 0) do_clear = 1;
                else if (strcmp(argv[i], "--dedup") == 0) do_dedup = 1;
                else if (strcmp(argv[i], "--limit") == 0 && i+1 < argc) limit = atoi(argv[++i]);
            }
            if (forget_id) mem_forget_cli(db, forget_id);
            else if (do_clear) mem_clear_all(db, kind);
            else if (do_dedup) { int n = mem_dedup(db); printf("deduplicated %d memories\n", n); }
            else if (search) mem_search(db, search, limit);
            else mem_list(db, kind, limit);
            vive_db_close(db);
            return 0;
        }
        if (strcmp(argv[1], "init") == 0) {
            int global = 0, remove = 0;
            for (int i = 2; i < argc; i++) {
                if (strcmp(argv[i], "--global") == 0) global = 1;
                if (strcmp(argv[i], "--remove") == 0) remove = 1;
            }
            char cwd[1024];
            getcwd(cwd, sizeof(cwd));
            return remove ? vive_init_remove(cwd, global) : vive_init(cwd, global);
        }
        if (strcmp(argv[1], "hook") == 0 && argc >= 3) {
            /* hooks: try project-scoped DB, fall back to global */
            const char *db_path = get_db_path();
            sqlite3 *db = NULL;
            if (vive_db_open(db_path, &db) != 0) {
                /* fall back to global */
                db_path = get_global_db_path();
                if (vive_db_open(db_path, &db) != 0) return 1;
            }
            sqlite3_exec(db, "PRAGMA busy_timeout = 5000;", NULL, NULL, NULL);

            /* read all stdin */
            size_t icap = 4096, ilen = 0;
            char *input = malloc(icap);
            for (;;) {
                int c = fgetc(stdin);
                if (c == EOF) break;
                if (ilen + 1 >= icap) { icap *= 2; input = realloc(input, icap); }
                input[ilen++] = (char)c;
            }
            input[ilen] = '\0';

            int rc = 0;
            if (strcmp(argv[2], "prompt") == 0) {
                rc = hook_prompt(db, input);
            } else if (strcmp(argv[2], "tool") == 0) {
                rc = hook_tool(db, input);
            } else if (strcmp(argv[2], "session-start") == 0) {
                char *out = hook_session_start(db, input);
                if (out) { printf("%s\n", out); free(out); }
            } else if (strcmp(argv[2], "stop") == 0) {
                rc = hook_stop(db, input);
            } else {
                fprintf(stderr, "unknown hook: %s\n", argv[2]);
                rc = 1;
            }

            free(input);
            vive_db_close(db);
            return rc;
        }
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    const char *db_path = get_db_path();
    if (vive_db_open(db_path, &g_db) != 0) {
        fprintf(stderr, "error: failed to open database at %s\n", db_path);
        return 1;
    }
    write_pid();

    int daemon_mode = (argc >= 2 && strcmp(argv[1], "--daemon") == 0);

    if (daemon_mode) {
        /* daemon mode: just run MCP on main thread, no TUI */
        fprintf(stderr, "vive: listening on stdio (daemon mode)\n");
        fprintf(stderr, "vive: db at %s\n", db_path);
        mcp_stdio_loop(g_db);
    } else {
        /* default: MCP on background thread, TUI in foreground */
        fprintf(stderr, "vive: starting server + TUI\n");
        fprintf(stderr, "vive: db at %s\n", db_path);

        pthread_t mcp_tid;
        pthread_create(&mcp_tid, NULL, mcp_thread, g_db);
        pthread_detach(mcp_tid);

        /* open a read-only connection for the TUI to avoid contention */
        sqlite3 *tui_db = NULL;
        if (vive_db_open_readonly(db_path, &tui_db) != 0) {
            /* fallback: share the main connection */
            tui_db = g_db;
        }

        tui_run(tui_db);

        if (tui_db != g_db) vive_db_close(tui_db);
    }

    vive_db_close(g_db);
    remove_pid();
    return 0;
}
