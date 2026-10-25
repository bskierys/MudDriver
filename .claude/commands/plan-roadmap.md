---
description: Convert the closed plan into ROADMAP.md entries
---

# /plan-roadmap

Role: product engineer converting an approved plan into concrete trackable work.

## Read first

1. `.claude/docs/PLAN.md` — must have a "Ready to convert to roadmap" verdict.
2. `.claude/docs/ROADMAP.md` — see existing phases.

## What to do

### 1. ROADMAP.md update

If the plan introduces a new phase: add a new `## Phase N — Name` section following the existing template (Outcome / Scope / Done when).

If the plan fits an existing phase: add or refine the relevant scope bullets.

### 2. Task quality rules

- Every task: 30 min – 4 h. Larger = split.
- Every task has done criteria inline.
- Every task that uses a new API or library gets a 🔍 Research first marker.
- Every task that has a non-obvious constraint or gotcha gets a ⚠️ Note marker.
- Every task that depends on another gets `_(requires SX-YY)_`.

### 3. Zero PLAN.md

Overwrite `.claude/docs/PLAN.md` back to its empty template (the section starting with "Single active plan…").

### 4. Append HISTORY.md entry

Add a snapshot:

```markdown
## YYYY-MM-DD — Sprint N planning

### Context
<one-paragraph summary of what this sprint is for>

### Tasks created
- SN-01 through SN-XX in SPRINTS.md

### Decisions made
<any ADR-worthy decisions made during planning — if so, also add to DECISIONS.md>
```

## Then tell the user

"Sprint N planned: X tasks in SPRINTS.md, ROADMAP.md updated, PLAN.md cleared. Run `/work` to start executing."
