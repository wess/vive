# Database Schema

Vive uses a single SQLite database with WAL mode for concurrent access between the MCP server, TUI, and hook processes.

Default location: `~/.local/share/vive/vive.db`

Override with: `VIVE_DB_PATH` environment variable.

## Tables

### tasks

| Column | Type | Description |
|--------|------|-------------|
| id | TEXT PK | 16-char hex ID |
| name | TEXT | Task name |
| state | TEXT | pending, running, completed, failed, cancelled |
| strategy | TEXT | sequential, parallel, supervised, statemachine |
| token_budget | INTEGER | Token ceiling |
| tokens_used | INTEGER | Tokens consumed |
| error_reason | TEXT | Error message (nullable) |
| created_at | INTEGER | Unix timestamp |
| updated_at | INTEGER | Unix timestamp |

### task_steps

| Column | Type | Description |
|--------|------|-------------|
| id | TEXT PK | 16-char hex ID |
| task_id | TEXT FK | References tasks(id) |
| step_index | INTEGER | Ordered position |
| state | TEXT | Step state |
| description | TEXT | Step description |
| started_at | INTEGER | Unix timestamp (nullable) |
| completed_at | INTEGER | Unix timestamp (nullable) |

### agents

| Column | Type | Description |
|--------|------|-------------|
| id | TEXT PK | 16-char hex ID |
| role | TEXT | Agent role |
| state | TEXT | idle, busy, error |
| current_task_id | TEXT FK | Currently assigned task (nullable) |
| tools | TEXT | JSON array of tool names (nullable) |
| token_budget | INTEGER | Token ceiling |
| tokens_used | INTEGER | Tokens consumed |
| error_reason | TEXT | Error message (nullable) |
| registered_at | INTEGER | Unix timestamp |

### memories

| Column | Type | Description |
|--------|------|-------------|
| id | TEXT PK | 16-char hex ID |
| content | TEXT | Memory content |
| kind | TEXT | semantic, structured, session |
| importance | REAL | Importance score (0.0+) |
| access_count | INTEGER | Times accessed |
| ttl | INTEGER | Time-to-live in seconds (nullable = permanent) |
| created_at | INTEGER | Unix timestamp |
| last_accessed | INTEGER | Unix timestamp |
| session_id | TEXT | Claude Code session ID (nullable, added via migration) |

#### Memory Tiers

| Tier | kind | importance | ttl | Source |
|------|------|-----------|-----|--------|
| Persistent | structured | 1.0 / 0.7 | NULL | Decision detection, file edits |
| Medium | semantic | 0.5 | 604800 (7d) | Session summaries |
| Ephemeral | session | 0.1 | 86400 (24h) | Prompts, tool calls |

### context_windows

| Column | Type | Description |
|--------|------|-------------|
| task_id | TEXT PK | References tasks(id) |
| budget | INTEGER | Token budget |
| used | INTEGER | Tokens used |
| compressed_size | INTEGER | Size after compression |
| full_size | INTEGER | Original size |

### sessions

| Column | Type | Description |
|--------|------|-------------|
| id | TEXT PK | 16-char hex ID |
| client | TEXT | Client identifier |
| connected_at | INTEGER | Unix timestamp |
| disconnected_at | INTEGER | Unix timestamp (nullable) |
| request_count | INTEGER | Total requests in session |

### request_log

| Column | Type | Description |
|--------|------|-------------|
| id | INTEGER PK | Auto-increment |
| session_id | TEXT FK | References sessions(id) |
| method | TEXT | MCP method name |
| latency_ms | INTEGER | Request latency |
| error | TEXT | Error message (nullable) |
| created_at | INTEGER | Unix timestamp |

## Indexes

- `idx_request_log_created` on request_log(created_at)
- `idx_memories_kind` on memories(kind)
- `idx_memories_session` on memories(session_id)
- `idx_tasks_state` on tasks(state)
- `idx_task_steps_task` on task_steps(task_id)

## Migrations

Schema initialization uses `CREATE TABLE IF NOT EXISTS` for all tables. The `session_id` column on memories is added via `ALTER TABLE` if not present — checked by inspecting `PRAGMA table_info(memories)`.

## Concurrency

- WAL mode enabled via `PRAGMA journal_mode=WAL` at schema init
- Hook processes use `PRAGMA busy_timeout = 5000` for write contention
- TUI opens a read-only connection to avoid blocking the server
- Multiple async hooks may write concurrently — SQLite WAL serializes writes automatically

## Memory Lifecycle

- Memories with `ttl` set are pruned when `created_at + ttl` exceeds current time
- Pruning runs every 100 MCP requests and at session end (Stop hook)
- Accessing a memory bumps `access_count` and `last_accessed`
- Context injection queries order by `importance DESC, access_count DESC`
