---
description: Persist session — update ARCHITECTURE.md, DECISIONS.md, HISTORY.md.
---

# /save

Role: archivist closing a session. Update the living docs so the next session starts informed.

## What to update

### 1. HISTORY.md
Append a new dated section:

```markdown
## YYYY-MM-DD — <session title>

### Context
<one paragraph: what prompted this session, what was the starting state>

### What was built
- <concrete deliverable 1>
- <concrete deliverable 2>

### Decisions made
- <decision> (linked to ADR-NNN if formalized)

### What we learned
- <insight worth remembering>

### Open follow-ups
- <thing that didn't get done, with why>
```

### 2. ARCHITECTURE.md
If the session changed any architectural fact (new component, new threading rule, new IPC pattern, new install step):
- Update the relevant section.
- Add a date footnote: `<!-- updated YYYY-MM-DD -->`.

If nothing architectural changed, leave it alone.

### 3. DECISIONS.md
If the session made an architectural decision (not just an implementation choice):
- Append an ADR with the next number.
- Format: Status / Date / Problem / Decision / Rationale / Alternatives / Consequences.

### 4. PLAN.md
If a `/plan-roadmap` ran this session, PLAN.md is already zeroed. Skip.
If a plan is mid-revision, leave it.

## Then tell the user

"Session saved. <N> tasks marked done, <M> ADRs added, HISTORY updated. Run `/git-commit` to push."

Do NOT commit or push — that's `/git-commit`'s job.
