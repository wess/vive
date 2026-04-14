# Core Concepts

## The Problem

AI agent tooling pipes entire conversations to models with no intelligence about what context is relevant. This wastes tokens, loses focus, and makes multi-agent coordination manual. Every new session starts from scratch.

## How Vive Helps

Vive is invisible infrastructure that makes AI sessions smarter over time:

- **Persistent memory** — decisions, file changes, and session summaries survive across sessions
- **Automatic context** — relevant memories are injected at session start, no re-explaining needed
- **Token budgeting** — each task gets a ceiling, preventing runaway context
- **Task orchestration** — define execution strategies and track progress
- **Agent coordination** — register agents, assign work, hand off context

## Seamless Integration

Vive uses two integration surfaces with Claude Code:

### Hooks (Automatic)

Claude Code hooks fire on events. Vive processes them silently:

| Hook | What Vive Does |
|------|---------------|
| `UserPromptSubmit` | Stores prompt, detects decisions via pattern matching |
| `PostToolUse` | Tracks file creates/edits, build commands |
| `SessionStart` | Injects relevant context, updates CLAUDE.md |
| `Stop` | Creates session summary, prunes expired memories |

### MCP Server (On Demand)

All 17 tools are available when Claude or the user wants explicit control — creating tasks, querying memories, managing agents, etc.

## Memory

### Three Tiers

**Persistent** (no expiry)
- Project decisions: "using Go for the API", "JWT for auth"
- File operations: "created src/auth.c", "modified include/vive.h"
- Detected automatically by regex patterns on user prompts and tool use
- Importance: 1.0 (decisions), 0.7 (file edits)

**Medium-lived** (7 days)
- Session summaries: "implemented user auth, created 5 files"
- Created automatically at session end
- Importance: 0.5

**Ephemeral** (24 hours)
- Individual user prompts
- Tool call records
- Importance: 0.1

### Decision Detection

Vive matches patterns in user prompts to identify decisions worth persisting:

- "let's use X" / "we'll go with X" / "the stack is X"
- "we decided X" / "the architecture is X"
- "don't X" / "never X" / "always X"

Minimum length thresholds reduce false positives. Missing a decision is worse than storing a non-decision — memories can be pruned.

### Importance and Ranking

Memories are ranked by `importance DESC, access_count DESC`. Each time a memory is accessed, its count bumps. Frequently-accessed memories rise to the top.

Context injection at session start picks the top N by this ranking, capped at ~500 tokens.

## Context Injection

### System Reminder

At session start, Vive returns a compact context block that Claude sees as a system reminder:

```
[Vive Context]
Last session: implemented user auth, created login/signup endpoints
Active tasks: 2 running, 1 pending
Decisions:
- Using Go for API, SQLite for storage
- JWT auth with bcrypt passwords
```

### CLAUDE.md

Vive also maintains a managed section in CLAUDE.md:

```markdown
<!-- vive:start -->
## Project Context (managed by Vive)

- Using Go for API, SQLite for storage
- JWT auth with bcrypt passwords
- created src/auth.c
<!-- vive:end -->
```

Content outside the markers is never touched.

## Tasks

A task is a unit of work with a name, strategy, and token budget.

### Execution Strategies

- **Sequential** — steps execute one after another
- **Parallel** — fan-out to multiple agents
- **Supervised** — quality gates between steps
- **State Machine** — define phases with transition rules

### Lifecycle

```
pending -> running -> completed
                   -> failed
          -> cancelled
```

## Agents

Agents represent workers assigned to tasks.

### States

- **Idle** — available for work
- **Busy** — assigned to a task
- **Error** — encountered a problem

### Handoff

`agent.handoff` transfers a task from one agent to another. The source becomes idle, the target inherits the assignment.

## Auto-Compression

When a task reaches 80% of its token budget, Vive automatically compresses completed step descriptions into a summary. This is extractive (first line of each step), not abstractive — no LLM calls needed.

Compressed summaries are stored as medium-lived memories and surfaced at the next session start.

Budgets are advisory. At 100%, Vive adds a warning to the next session context but never blocks work.

## Architecture

Vive runs as a single binary with multiple modes:

- **`vive`** — MCP server on background thread + ncurses TUI in foreground
- **`vive --daemon`** — MCP server on stdio only
- **`vive hook <event>`** — short-lived process for Claude Code hooks

All modes share the same SQLite database (WAL mode for concurrent access). Hook processes use `busy_timeout` to handle write contention.
