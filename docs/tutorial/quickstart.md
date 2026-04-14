# Quick Start

Get Vive running in under 5 minutes.

## Install

### Prerequisites

```sh
# macOS
brew install cjson

# Goose build system
# See https://github.com/wess/goose
```

### Build

```sh
git clone https://github.com/your/vive.git
cd vive
goose build
```

## Set Up Claude Code Integration

```sh
vive init
```

This single command:
- Registers Vive as an MCP server
- Installs hooks for auto-memory and context injection
- Creates a `/vive` slash command
- Adds a managed section to CLAUDE.md

Restart Claude Code after running this.

For all projects instead of just the current one:

```sh
vive init --global
```

## That's It

Vive is now running invisibly. Here's what happens automatically:

1. **You start a session** — Vive injects relevant memories from past sessions as context
2. **You make decisions** — "let's use Go for the API" is detected and stored permanently
3. **You create files** — Vive tracks what was created and modified
4. **You end a session** — Vive summarizes the work and prunes expired memories
5. **Next session** — all of the above is surfaced automatically

No manual tool calls needed.

## Optional: Use Tools Directly

You can also interact with Vive explicitly:

```
/vive create a task called build-auth-system
/vive store this memory: the API uses JWT tokens with 24h expiry
/vive show me all running tasks
```

Or use the MCP tools directly — all 17 are available to Claude.

## Running the TUI

To see what Vive is doing in real-time:

```sh
vive
```

This starts the MCP server with a live ncurses dashboard. Press `q` to quit.

For daemon mode (no TUI):

```sh
vive --daemon
```

For a quick status check:

```sh
vive status
```

## Configuration

### Database Location

Default: `~/.local/share/vive/vive.db`

Override:
```sh
export VIVE_DB_PATH=/path/to/vive.db
```

### Release Build

```sh
goose build --release
goose install
```
