# Vive

A smart harness server for AI agent orchestration. Manages context, memory, and task routing so agents stay focused and token-efficient.

## What It Does

Vive sits between AI clients and model providers as invisible infrastructure. Once set up, it automatically:

- **Remembers decisions** — detects and persists project decisions across sessions
- **Tracks file changes** — knows what was created and modified
- **Injects context** — at session start, surfaces relevant memories so you never re-explain
- **Manages tasks** — create, run, cancel, and monitor tasks with execution strategies
- **Orchestrates agents** — register agents, assign tasks, handoff context between them
- **Shows you what's happening** — live ncurses TUI dashboard with 5 panels

## Quick Start

### Prerequisites

- C compiler (cc/gcc/clang)
- [Goose](https://github.com/wess/goose) build system
- SQLite3, cJSON, ncurses (via Homebrew: `brew install cjson`)

### Build and Set Up

```sh
goose build
./build/debug/vive init
```

That's it. Restart Claude Code and Vive is active — auto-memory, auto-context, all 17 MCP tools available.

### Install System-Wide

```sh
goose build --release
goose install
vive init --global  # enable for all projects
```

## How It Works

### Seamless Integration

`vive init` configures everything in one shot:

1. Registers Vive as an MCP server (Claude Code can call all 17 tools directly)
2. Installs hooks that automatically capture events:
   - **UserPromptSubmit** — stores prompts, detects decisions
   - **PostToolUse** — tracks file changes, build commands
   - **SessionStart** — injects relevant context as a system reminder + updates CLAUDE.md
   - **Stop** — creates session summary, prunes expired memories
3. Creates a `/vive` slash command for natural language interaction
4. Adds a managed section to CLAUDE.md for durable project context

### Auto-Memory (3 Tiers)

| Tier | What | TTL | Example |
|------|------|-----|---------|
| Persistent | Decisions, file changes | forever | "using Go for API", "created src/auth.c" |
| Medium | Session summaries | 7 days | "implemented user auth, 5 files created" |
| Ephemeral | Prompts, tool calls | 24 hours | "ran goose build", prompt text |

Decisions are detected automatically via pattern matching — "let's use X", "we decided Y", "the stack is Z". No manual `memory.store` calls needed.

### Auto-Context

At every session start, Vive injects a compact summary:

```
[Vive Context]
Last session: implemented user auth, created login/signup endpoints
Active tasks: 2 running, 1 pending
Decisions:
- Using Go for API, SQLite for storage
- JWT auth with bcrypt passwords
```

It also updates CLAUDE.md with durable project knowledge between `<!-- vive:start -->` / `<!-- vive:end -->` markers.

## Running Vive

```sh
# Start with live TUI dashboard (MCP server runs on background thread)
vive

# Start in daemon mode (no TUI, stdio only)
vive --daemon

# Check status
vive status

# Set up Claude Code integration
vive init              # project-scoped
vive init --global     # all projects
```

### TUI Dashboard

When run without `--daemon`, Vive shows a live monitoring dashboard:

```
+- Tasks -------------------+- Agents ------------------+
| * 3 running  7 queued     | * worker-1   busy [task-42]|
| + 24 completed  x 1 failed| * worker-2   busy [task-38]|
|                            |   reviewer   idle          |
| [task-42] migrate-schema   |                            |
|   step 3/5  ======--  62% |                            |
+- Tokens ------------------+- Memory ------------------+
| budget: 120k   used: 84k  | total: 1,247 memories     |
| compressed: 31k saved     | semantic: 892  struct: 355|
+- System ---------------------------------------------- +
| uptime: 2h 14m | requests: 1,247 | avg latency: 23ms  |
+-------------------------------------------------------|
```

Press `q` to quit.

## MCP Tools

All 17 tools are available to any MCP client. With Claude Code, use them directly or via `/vive`:

```
/vive create a task called migrate-schema
/vive store this as a memory: the API uses JWT tokens
/vive show me all running tasks
```

| Category | Tool | Description |
|----------|------|-------------|
| Task | `task.create` | Create a task with strategy and token budget |
| Task | `task.run` | Execute a task |
| Task | `task.status` | Get task status with step progress |
| Task | `task.cancel` | Cancel a running task |
| Task | `task.list` | List tasks with optional state filter |
| Memory | `memory.store` | Persist content with kind and TTL |
| Memory | `memory.query` | Search memories by content |
| Memory | `memory.forget` | Delete a memory |
| Memory | `memory.summarize` | Get raw content for client-side summarization |
| Context | `context.compress` | Get completed steps for client-side compression |
| Context | `context.retrieve` | Get context window stats |
| Context | `context.budget` | Set token budget for a task |
| Context | `context.inject` | Add compressed context back |
| Agent | `agent.register` | Register an agent with role |
| Agent | `agent.handoff` | Transfer context between agents |
| Agent | `agent.spawn` | Create agent assigned to a task |
| Agent | `agent.status` | Check agent state |

## Technology

- **Language**: C11
- **Build**: [Goose](https://github.com/wess/goose)
- **Database**: SQLite (WAL mode)
- **Protocol**: MCP (JSON-RPC) over stdio
- **JSON**: cJSON
- **TUI**: ncurses

## Project Structure

```
vive/
  goose.yaml           build config
  include/vive.h       public header
  src/
    main.c             entry point (MCP thread + TUI + subcommands)
    init.c             vive init — Claude Code integration setup
    hook.c             hook handlers (prompt, tool, session-start, stop)
    types.c            type helpers
    schema.c           SQLite schema and migrations
    db.c               database queries
    protocol.c         JSON-RPC serialization
    mcp.c              MCP request dispatch
    context.c          context manager
    memory.c           memory store + query functions
    router.c           task router and agent management
    tui.c              ncurses dashboard
  plugins/claude/      Claude Code plugin files
  docs/
    api/               API reference
    tutorial/          Getting started guides
```

## License

MIT
