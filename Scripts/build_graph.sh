#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
WORKTREES_DIR="$(realpath -m "${REPO_ROOT}/../worktrees")"

RED=$'\033[0;31m'
GREEN=$'\033[0;32m'
BLUE=$'\033[0;34m'
BOLD=$'\033[1m'
NC=$'\033[0m'

if command -v graphify >/dev/null 2>&1; then
    GRAPHIFY_BIN="graphify"
elif [ -x "${HOME}/.local/share/uv/tools/graphifyy/bin/graphify" ]; then
    GRAPHIFY_BIN="${HOME}/.local/share/uv/tools/graphifyy/bin/graphify"
elif [ -x "${HOME}/.local/bin/graphify" ]; then
    GRAPHIFY_BIN="${HOME}/.local/bin/graphify"
else
    echo -e "${RED}Error: 'graphify' CLI not found. Please install via: uv tool install graphifyy${NC}" >&2
    exit 1
fi

echo -e "${BOLD}${BLUE}==> Using Graphify:${NC} ${GRAPHIFY_BIN}"

COMPONENTS=(
    "autopilot"
    "gcs"
    "mavlink"
    "simulation"
    "swarm"
    "tracking"
    "voice"
)

GRAPH_FILES=()

echo ""
echo -e "${BOLD}--- [main repository] ---${NC}"
mkdir -p "${REPO_ROOT}/graphify-out/root"
"${GRAPHIFY_BIN}" extract "${REPO_ROOT}" --code-only --out "${REPO_ROOT}/graphify-out/root"
if [ -f "${REPO_ROOT}/graphify-out/root/graph.json" ]; then
    GRAPH_FILES+=("${REPO_ROOT}/graphify-out/root/graph.json")
fi

for comp in "${COMPONENTS[@]}"; do
    target_dir="${WORKTREES_DIR}/${comp}"
    if [ ! -d "${target_dir}" ]; then
        echo -e "Worktree directory '${target_dir}' does not exist, skipping."
        continue
    fi

    echo ""
    echo -e "${BOLD}--- [Worktree: ${comp}] ---${NC}"
    "${GRAPHIFY_BIN}" extract "${target_dir}" --code-only
    
    comp_graph="${target_dir}/graphify-out/graph.json"
    if [ -f "${comp_graph}" ]; then
        GRAPH_FILES+=("${comp_graph}")
    fi
done

echo ""
echo -e "${BOLD}${BLUE}==> Merging all component graphs into root knowledge graph...${NC}"
mkdir -p "${REPO_ROOT}/graphify-out"

"${GRAPHIFY_BIN}" merge-graphs "${GRAPH_FILES[@]}" --out "${REPO_ROOT}/graphify-out/graph.json"

echo ""
echo -e "${BOLD}${BLUE}==> Exporting HTML visualization...${NC}"
GRAPHIFY_VIZ_NODE_LIMIT=10000 "${GRAPHIFY_BIN}" export html --node-limit 10000 --graph "${REPO_ROOT}/graphify-out/graph.json"

echo ""
echo -e "${BOLD}${GREEN}==> Knowledge graph successfully built!${NC}"
echo "Graph data: ${REPO_ROOT}/graphify-out/graph.json"
echo "HTML view:  ${REPO_ROOT}/graphify-out/graph.html"
