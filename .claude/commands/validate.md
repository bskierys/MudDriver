---
description: Senior engineer review of the current state. Run before starting a new phase or before a release.
---

# /validate

Role: senior UE5 engineer doing a code review of the current state.

## Read first

1. `.claude/docs/ARCHITECTURE.md` — what we said we'd build.
3. `.claude/docs/DECISIONS.md` — what constraints we committed to.
4. Walk the actual `Source/` and `Tools/` trees — look at what's there now.

## Review dimensions

For each, write a paragraph with concrete evidence. Cite file paths and line numbers.

### 1. C++/UE5 hygiene
- Do we follow best UE5 practices?
- Are hard references used where we can have soft references?
- Do a code have a good performance?
- Any `UE_LOG(LogTemp, …)` slipping in? Should be our custom or even feature specific log with `LogB1` prefix. e.g. `LogB1Physics`.

### 2. Debugging
- Is there a proper debugging tool for new feature for testers, designers to work with? Prefer ImGui and ShowDebug over Utility widgets but they are also fine if no other option. THe goal is to be able to validate, test and play with the feature at runtime.
- Is there enough but not too much logs so that we can check for errors in runtime?

### 3. Documentation freshness
- Any new pattern that should be a skill but isn't?
- Any architectural decision made in code that lacks an ADR?

### 4. Build / CI health
- Does UnrealBuildTool pass clean on a fresh project?
- Any TODO / FIXME comments in shipped code without tickets?

## Output

A markdown report under headers above. End with:

```markdown
## Verdict
- [ ] Ready to ship / advance to next phase
- [ ] Blocking issues:
  - <issue>
  - <issue>
- [ ] Non-blocking but recommended:
  - <item>
```

Save the report inline in the chat — do NOT commit it as a file (it's a snapshot of one moment).

If verdict is "ready": tell the user "validated, you can proceed with [next phase / release]".
If "blocking": tell the user "blocking issues found, address before [advancing]".
