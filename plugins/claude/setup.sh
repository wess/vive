#!/bin/sh
# Register Vive as an MCP server for Claude Code
# Usage: ./plugins/claude/setup.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VIVE_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VIVE_BIN="$VIVE_ROOT/build/debug/vive"

if [ ! -f "$VIVE_BIN" ]; then
    echo "building vive..."
    cd "$VIVE_ROOT" && goose build
fi

# Register MCP server
echo "registering vive as MCP server..."
claude mcp add vive "$VIVE_BIN" -- --daemon

# Symlink slash commands
COMMANDS_DIR="$HOME/.claude/commands"
mkdir -p "$COMMANDS_DIR"

for cmd in "$SCRIPT_DIR/commands"/*.md; do
    name="$(basename "$cmd")"
    ln -sf "$cmd" "$COMMANDS_DIR/$name"
    echo "linked /$name command"
done

echo ""
echo "done! vive is now available as an MCP server."
echo "use /vive in Claude Code to interact with it."
