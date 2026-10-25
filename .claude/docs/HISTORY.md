# HISTORY

Session snapshots, newest at the bottom. Grown by `/save` and `/plan-roadmap`.

---

## 2026-10-25 — Sprint 1 planning

### Context
First planning pass for the VISION.md example scenario, the river crossing from Huta to Bagnisko. The project started as the UE vehicle template plus a NetImGui menu. Design questions were settled in Q&A, the plan went through `/plan-new`, `/plan-analyse` (needs revision) and `/plan-replan` (ready), and is now converted into ROADMAP Phases 0–5 plus Sprint 1. Sprint 1 goes after the two biggest unknowns: the Chaos simulation hook and mud sinking, tested in a gym with agent-runnable test rigs. T0.1 (render budget) was done during planning: the user restarted the editor and was happy with the result (no `stat unit` numbers recorded).

### Tasks created
- S1-01 through S1-24 in SPRINTS.md (S1-01 already done)
- Sprint 2 outlined (GAS + fuel + mud polish), to be detailed at sprint start

### Decisions made
- ADR-001 dynamic per-wheel mud sinking
- ADR-002 mud params in a custom physical material (amended by ADR-012)
- ADR-003 environment forces inside the Chaos simulation (amended: stepping mode decided in S1-12, template constructor change)
- ADR-004 single ASC on PlayerState; the jeep holds a weak ref (user's call, overriding the two-ASC proposal)
- ADR-005 single Condition attribute, no fail state
- ADR-006 obstacles without ASC
- ADR-007 winch as a capped tension force, Uncharted-style
- ADR-008 visual-only mud trails
- ADR-009 gyms before the scenario level
- ADR-010 fuel attribute consumed by engine work
- ADR-011 low-end render budget
- ADR-012 mud collision layering + sinking via suspension trace offset
- Testing model: agent rig tests (`AB1GymTestRig : AFunctionalTest`, PIE via MCP or headless) + a user check per task + a playtest sign-off per phase; feel wins over thresholds
- Target machine: i7-12700H, 32 GB RAM, RTX 3050 Ti Laptop 4 GB. Target 60 fps at 1080p Medium
- Phase 4: the character is hidden on the seat while driving; the engine switches off on exit

### Open follow-ups
- gdep MCP was failing this session (`mcp` is now pinned to 1.30.0). Claude Code restart planned before `/work`.
- No `stat unit` baseline for T0.1. The first numbers come from the S1-24 playtest.
