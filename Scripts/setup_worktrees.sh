#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
WORKTREES_DIR="$(realpath -m "${REPO_ROOT}/../worktrees")"

RED=$'\033[0;31m'
GREEN=$'\033[0;32m'
BLUE=$'\033[0;34m'
BOLD=$'\033[1m'
NC=$'\033[0m'

COMPONENTS=(
    "autopilot:autopilot/master"
    "gcs:gcs/master"
    "mavlink:mavlink/master"
    "simulation:simulation/master"
    "swarm:swarm/master"
    "tracking:tracking/master"
    "voice:voice/master"
)

echo -e "${BOLD}${BLUE}==> Setting up worktrees in:${NC} ${WORKTREES_DIR}"
mkdir -p "${WORKTREES_DIR}"

for comp in "${COMPONENTS[@]}"; do
    name="${comp%%:*}"
    branch="${comp##*:}"
    target_dir="${WORKTREES_DIR}/${name}"

    echo ""
    echo -e "${BOLD}--- [${name}] (${branch}) ---${NC}"

    if [ -d "${target_dir}" ]; then
        echo -e "Directory '${target_dir}' already exists. Skipping creation."
    else
        echo "Creating worktree at '${target_dir}'..."
        if git show-ref --verify --quiet "refs/heads/${branch}"; then
            git worktree add "${target_dir}" "${branch}"
        elif git show-ref --verify --quiet "refs/remotes/origin/${branch}"; then
            git worktree add --track -b "${branch}" "${target_dir}" "origin/${branch}"
        else
            echo -e "${RED}Error: Branch '${branch}' not found locally or on origin.${NC}" >&2
            continue
        fi
    fi

    if [ -f "${target_dir}/.gitmodules" ]; then
        echo "Updating submodules for ${name}..."
        git -C "${target_dir}" submodule update --init --recursive
    fi
done

echo ""
echo -e "${BOLD}${GREEN}==> All worktrees and submodules are ready!${NC}"
