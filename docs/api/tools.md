# Vive MCP Tools Reference

Vive exposes 17 tools via the Model Context Protocol (JSON-RPC over stdio).

With Claude Code, these tools are available automatically after `vive init`. Use them directly or via the `/vive` slash command.

## Task Tools

### task.create

Create a new task.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| name | string | yes | Task name |
| strategy | string | no | `sequential` (default), `parallel`, `supervised`, `statemachine` |
| token_budget | int | no | Maximum tokens allocated |

**Response:**

```json
{
  "id": "a1b2c3d4e5f6g7h8",
  "name": "migrate-schema",
  "state": "pending",
  "strategy": "sequential",
  "token_budget": 40000,
  "tokens_used": 0,
  "created_at": 1775584502,
  "updated_at": 1775584502
}
```

### task.run

Start executing a task.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| task_id | string | yes | Task ID |

### task.status

Get detailed task status including step progress.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| task_id | string | yes | Task ID |

Response includes a `steps` array with per-step progress.

### task.cancel

Cancel a running or pending task.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| task_id | string | yes | Task ID |

### task.list

List tasks, optionally filtered by state.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| filter | string | no | `pending`, `running`, `completed`, `failed`, `cancelled` |

## Memory Tools

### memory.store

Store a new memory. In most cases, Vive stores memories automatically via hooks. Use this for explicit storage.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| content | string | yes | Memory content |
| kind | string | no | `semantic`, `structured` (default), `session` |
| ttl | int | no | Time-to-live in seconds. 0 = permanent |

### memory.query

Search memories by content (substring match).

| Name | Type | Required | Description |
|------|------|----------|-------------|
| text | string | yes | Search text |
| limit | int | no | Max results (default: 10) |

Accessing a memory bumps its `access_count` and `last_accessed`, increasing its importance ranking.

### memory.forget

Delete a specific memory.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| id | string | yes | Memory ID |

### memory.summarize

Retrieve raw content of multiple memories for client-side summarization. Vive does not call LLMs — the client runs the content through a model and stores the result back via `memory.store`.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| ids | string[] | yes | Array of memory IDs |

## Context Tools

### context.compress

Get completed task step descriptions for client-side compression.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| task_id | string | yes | Task ID |

Returns concatenated step descriptions. Auto-compression at 80% budget is handled by hooks.

### context.retrieve

Get context window statistics for a task.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| task_id | string | yes | Task ID |

### context.budget

Set or update the token budget for a task.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| task_id | string | yes | Task ID |
| limit | int | yes | Token budget |

### context.inject

Add compressed context back to a task's context window.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| task_id | string | yes | Task ID |
| tokens | int | yes | Number of tokens in the injected content |

## Agent Tools

### agent.register

Register a new agent.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| role | string | yes | Agent role (e.g., "code-reviewer", "planner") |
| token_budget | int | no | Token budget |

### agent.handoff

Transfer a task from one agent to another.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| from_agent_id | string | yes | Source agent ID |
| to_agent_id | string | yes | Target agent ID |

### agent.spawn

Create a new agent and immediately assign it to a task.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| role | string | yes | Agent role |
| task_id | string | yes | Task to assign |

### agent.status

Get agent status.

| Name | Type | Required | Description |
|------|------|----------|-------------|
| agent_id | string | yes | Agent ID |

## Error Handling

All errors follow JSON-RPC format:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "error": { "code": -32602, "message": "missing task_id" }
}
```

| Code | Meaning |
|------|---------|
| -32700 | Parse error (invalid JSON) |
| -32600 | Invalid request (missing method) |
| -32601 | Method not found |
| -32602 | Invalid params (missing required field) |
| -32603 | Internal error (database failure) |
