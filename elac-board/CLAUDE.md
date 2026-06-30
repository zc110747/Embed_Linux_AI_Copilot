# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```bash
# Build the Go client
go build -o main cmd/main.go

# Install dependencies
go mod tidy

# Run the test server (Python) — requires pip install websockets
python3 tests/server.py                 # automated test mode
python3 tests/server.py --interactive   # manual input mode
```

No unit test framework is configured yet. The only test is the Python WebSocket test server in `tests/server.py`, which sends requests and validates responses.

## Architecture

**ELAC Board** is an embedded Linux diagnostic agent. It connects to a remote server via WebSocket (as a client), receives JSON-RPC-style diagnostic requests, executes shell commands on the board, and returns results.

### Request flow

```
Server → WebSocket → transport (read loop) → handler → Match action
                                                          ├── "ping"    → pong response
                                                          ├── "collect" → collector.CollectHandler
                                                          └── default   → generic ok response
```

### Collector DSL

The `collector` package is a mini diagnostic engine. A collector is defined as a JSON file (see `configs/features/i2c-diagnose.json`) containing:
- **params** — default parameters with `{{placeholder}}` template syntax
- **steps** — ordered list of `{name, type, input}` where type is `exec` (shell command) or `analyze`

At runtime, the server sends `{"collector": "i2c-diagnose", "bus": 1}` in the payload. The collector loads the JSON definition, merges parameters, renders templates, and executes each step via `sh -c`. Results are aggregated into a structured response with per-step status/output and a simple pass/fail analysis.

Currently only the `i2c-diagnose` collector is hardcoded in `loadCollector()`. The `runtime/workflow.go` is a stub reserved for future multi-step workflow orchestration (per `docs/plans.md`: v0.2 Workflow, v0.3 Script Engine).

### Key packages

| Package | Role |
|---------|------|
| `cmd/main.go` | Entry point: init app, handle OS signals, reconnect on disconnect |
| `internal/app` | Config loading (YAML), wiring transport+handler, lifecycle (Start/Stop/Reconnect) |
| `internal/transport` | WebSocket client with TLS config, read loop, send with mutex, reconnect with retry |
| `internal/protocol` | JSON request/response structs and constructors |
| `internal/collector` | Collector DSL engine: load JSON defs, shell exec, template render, result aggregation |
| `internal/utils` | Level-filtered logger and JSON pretty-print helpers |

### Configuration

`configs/board.yaml` — server URL, reconnect settings, TLS (insecure skip verify), log level. Defaults are set in code and overridden by the YAML file.

### Protocol

Described in `docs/protocol.md`. Simple JSON request/response over WebSocket text frames. Every message carries `version`, `id`, `session-id`, `type`, and a variable `payload`. Responses add `status` ("ok" or "error").

### Roadmap (from `docs/plans.md`)

v0.1 WebSocket + Collector → v0.2 Workflow → v0.3 Script Engine → v0.4 Diagnose → v1.0 AI Copilot
