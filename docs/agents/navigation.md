# Codebase Navigation & Knowledge Graph

Guidelines for agents exploring, querying, and navigating this multi-worktree repository using the unified Graphify knowledge graph.

## Architecture & Worktree Topology

The project is partitioned across git worktrees located at `../worktrees/` relative to the repository root:

- **Root repository (`ceng-407-408-...`):** Project documentation (`docs/`), domain models (`GLOSSARY.md`), orchestration scripts (`Scripts/`), and unified graph storage (`graphify-out/`).
- **Worktree modules (`../worktrees/<module>`):**
  - `autopilot`: PX4-based flight control algorithms, uORB messages, and navigation modules.
  - `gcs`: QGroundControl desktop application, Qt/QML UI, audio pipeline, and voice handling.
  - `mavlink`: MAVLink protocol definitions and pymavlink generator bindings.
  - `simulation`: SITL / Gazebo simulation environments.
  - `swarm`: Decentralized swarm coordination and consensus algorithms.
  - `tracking`: Target tracking, computer vision, and gimbal control pipelines.
  - `voice`: Audio preprocessing and speech-to-intent inference engines.

All modules are unified into a single persistent knowledge graph at `graphify-out/graph.json`.

---

## The Golden Rule

**Query the graph first before broad file searches.**

With multiple decoupled worktrees and over 140,000 entities, broad `grep` or sequential file reads create heavy context load and miss cross-worktree relationships. The graph provides deterministic AST links and pre-resolved cross-repo dependencies.

---

## Agent Navigation Tooling

Agents should navigate the codebase through either the native MCP tools (preferred) or the CLI fallback.

### 1. Via MCP Server (`graphify`)

When the `graphify` MCP server is active, call MCP tools directly:

| Task | MCP Tool | Example Arguments |
| :--- | :--- | :--- |
| **Search concept / flow** | `query_graph` | `{"question": "VoiceCommandManager handling", "token_budget": 1500}` |
| **Inspect symbol details** | `get_node` | `{"name": "VoiceCommandManager"}` |
| **Find callers / callees** | `get_neighbors` | `{"node_id": "<id>", "direction": "both"}` |
| **Trace dependency path** | `shortest_path` | `{"source": "VoiceCommandManager", "target": "MAVLink"}` |
| **Discover core hubs** | `god_nodes` | `{"top_n": 10}` |
| **Graph metadata & health** | `graph_stats` | `{}` |

### 2. Via CLI Fallback

When executing shell commands or scripting:

```bash
# Broad contextual traversal (BFS)
graphify query "MAVLink message handling"

# Trace specific call path (DFS)
graphify query "TakeoffHandling" --dfs

# Find shortest path between components across worktrees
graphify path "VoiceCommandManager" "MAVLink"

# Plain-language symbol explanation
graphify explain "VoiceCommandManager"
```

---

## Graph Maintenance & Rebuilds

The graph does not rebuild on individual git commits (git hooks are disabled).

### When to Rebuild
- After introducing new modules, significant architectural changes, or modifying cross-worktree interfaces (e.g. MAVLink dialects or shared message definitions).
- When a query reports missing newly added symbols.

### Rebuild Command
Run the root orchestration script:
```bash
./Scripts/build_graph.sh
```

This script:
1. Incrementally extracts AST symbols for each worktree honoring module-specific `.graphifyignore` rules (excluding third-party OS kernels like NuttX, virtualenvs, and build outputs).
2. Merges all component graphs into `graphify-out/graph.json`.
3. Resolves cross-repository type declarations and member calls.
4. Generates an updated HTML visualization at `graphify-out/graph.html`.

---

## Artifact Hygiene

- `graphify-out/` and `.graphify*` files are strictly local developer artifacts.
- Both `.gitignore` and `.git/info/exclude` prevent graph data from polluting git status across the root repo and worktrees. Never force-add or commit `graphify-out/*.json` files to git.
