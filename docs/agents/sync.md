# Synchronization: Issues, Wiki Requirements, and ADRs

Maintain strict consistency across the project's three canonical sources of truth whenever architecture, requirements, or task boundaries change:

1. **Architectural Decision Records (`docs/adr/`)**: Architectural rationale, technical constraints, module seams, and design trade-offs.
2. **Wiki Requirements (`../<repo>.wiki/Requirement,-Functionalities-and-System-Architecture.md`)**: Formal system specifications, MoSCoW prioritization tags, and numbered requirement definitions (`REQ-CORE-XXX`).
3. **GitHub Issues (`gh` CLI)**: Actionable work tracking items (`[EPIC]`, `[REQ]`, `[TASK]`), milestone associations, and Definitions of Done (DoD).

## The Golden Rule

**No orphaned changes.** An architectural decision, requirement revision, or module redesign is not complete until all three artifacts reflect the same reality. If you modify one, you must verify and synchronize the other two before closing the turn.

## Trigger & Propagation Matrix

| Trigger | Step 1: ADRs | Step 2: Wiki Requirements | Step 3: GitHub Issues |
| :--- | :--- | :--- | :--- |
| **Architectural / Seam Decision** | Create or update ADR in `docs/adr/` capturing context, decision, and rationale. | Add/update numbered `REQ-CORE-XXX` specifications to reflect the decision. | Update parent `[REQ]` issue body; create or update `[TASK]` issues with matching DoD. |
| **Requirement Change** | Check for conflicts with existing ADRs. Record new ADR if architectural. | Update requirement wording, MoSCoW classification, and acceptance criteria in the wiki. | Update corresponding `[REQ]` issue and child `[TASK]` scopes via `gh issue edit`. |
| **Technical Constraint Discovered in Task** | Resolve via ADR (or ADR update) if structural or cross-cutting. | Update requirement specifications in the wiki if functional contract shifted. | Update task DoD, story points, and parent requirement issue. |

## Workflow Checklist for Agents

### 1. ADR Update (`docs/adr/`)
- Check existing ADRs in `docs/adr/`.
- If refining an active decision within scope, update the existing ADR directly.
- If reversing or replacing a past decision, record a new ADR explicitly referencing the superseded ADR.

### 2. Wiki Requirements Update (`*.wiki/`)
- Edit `Requirement,-Functionalities-and-System-Architecture.md` in the wiki repository (`/home/bafti/ceng-final-project/<repo>.wiki/`).
- Ensure new requirements receive a sequential `REQ-CORE-XXX` identifier, clear description, and MoSCoW tag under the appropriate subsystem heading (`### Sesli Komut`, `### Sürü`, etc.).
- Update corresponding functional bullet points under `## Fonksiyonellikler`.
- Commit and push changes to the wiki remote: `git -C <wiki_path> commit -am "..." && git -C <wiki_path> push origin master`.

### 3. GitHub Issues Synchronization (`gh`)
- **Parent Requirement (`[REQ]`):**
  - Update `Requirement Details` and `Acceptance Criteria` using `gh issue edit <id> --body "..."` to include the new/updated `REQ-CORE-XXX` definitions.
- **Child Tasks (`[TASK]`):**
  - If new implementation work is identified, create a task issue: `gh issue create --title "[TASK]: ..." --label "Tasks" --milestone "<milestone>" --body "..."`.
  - Always include `### Parent Requirement\n\n#<parent_id>` in the body so repository GitHub Actions automatically maintain issue hierarchy.
  - If existing tasks change scope or location (e.g. moving validation from utility to coordinator module), update their implementation details and DoD using `gh issue edit <id> --body "..."`.
