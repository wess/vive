# /vive

Interact with the Vive orchestration server.

## Usage

Use Vive's MCP tools to manage tasks, agents, memory, and context. Vive runs as an MCP server — all tools are available directly.

## Quick Reference

### Tasks
- **Create a task**: `task.create` with name, strategy (sequential/parallel/supervised/statemachine), token_budget
- **Run a task**: `task.run` with task_id
- **Check status**: `task.status` with task_id (includes step progress)
- **Cancel**: `task.cancel` with task_id
- **List all**: `task.list` with optional filter (pending/running/completed/failed)

### Memory
- **Store**: `memory.store` with content, kind (semantic/structured/session), optional ttl (seconds)
- **Query**: `memory.query` with text (searches by content), optional limit
- **Forget**: `memory.forget` with id
- **Summarize**: `memory.summarize` with ids array — returns raw content for client-side summarization

### Context
- **Compress**: `context.compress` with task_id — returns completed step content for client-side compression
- **Retrieve**: `context.retrieve` with task_id — returns context window stats
- **Budget**: `context.budget` with task_id and limit (token ceiling)
- **Inject**: `context.inject` with task_id, content, tokens — add compressed context back

### Agents
- **Register**: `agent.register` with role, optional token_budget
- **Spawn**: `agent.spawn` with role and task_id — creates agent assigned to a task
- **Handoff**: `agent.handoff` with from_agent_id and to_agent_id
- **Status**: `agent.status` with agent_id

## Argument: $ARGUMENTS

If the user provided arguments, interpret them as a natural language request and use the appropriate Vive MCP tools to fulfill it. Examples:

- "create a task called migrate-schema" → use task.create
- "store this as a memory: the API uses JWT tokens" → use memory.store
- "show me all running tasks" → use task.list with filter "running"
- "register a code-reviewer agent" → use agent.register
