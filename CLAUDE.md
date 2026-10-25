# mcp-unreal-agent

MudDriver prototype of a game about offroad driving with rich mud and other obstacles physics.

This file is the **agent entry point**. Both Claude Code and GitHub Copilot read it (Copilot via [.github/copilot-instructions.md](.github/copilot-instructions.md) which delegates here). Cursor and other agents read it via [AGENTS.md](AGENTS.md).

---

## Tech stack

| Layer | Tech |
|---|---|
| Unreal Engine | 5.8 |

---

## Game technical overview

1. **Gameplay Ability System.** I want to base my game on GAS. So when in doubt, use patterns from this framework.
2. **Best practices** I want to develop using good UE5 practices. When in doubt Use LyraStarterGame for source of good practices.
3. **Physics, not great graphics** I don't care that much about AAA quality of graphics, in fact I want to have all recources for physics calculation and keep the graphics simple. To the degree that physics make sense of course. I want to have mud effects launching from tires when they spin in the mud, but I don't care about ray tracing lights etc. I have a low-end machine and I want the game to run on it.
4. **Debug tools** I want to have proper debug tools for features that we are building. I want to use NetImGui as primary debugging tool, but other tools that good are: `ShowDebug` command to display text over a vehicle or obstacle. Cheat commands, Utility widgets to modify or track parameters (if can't be done with NetImGui).

---

## Code structure and search

- The project code structure overview is managed by the tool called gdep. Use `\.gdep\AGENTS.md` as an entry point.
- The project uses Unreal MCP that allows you to search and edit Unreal files (blueprints, assets, etc.)
- Conventions: For custom classes, function categories, we prefix with `B1`. E.g. class name: `B1PlayerController`, Example function/property category `Mud|Config`. Example log category: `LogB1Physics`.

---

## Project documentation

All living docs are in [.claude/docs/](.claude/docs/). **`ARCHITECTURE.md` is the bible** — read it before touching any domain, keep it current via `/save`.

| File | Purpose |
|---|---|
| [.claude/docs/ARCHITECTURE.md](.claude/docs/ARCHITECTURE.md) | System design bible — components, threading, IPC, tool contract, C++ bridge, install flow |
| [.claude/docs/DECISIONS.md](.claude/docs/DECISIONS.md) | ADRs — why we chose X over Y |
| [.claude/docs/PLAN.md](.claude/docs/PLAN.md) | Single active plan, zeroed by `/plan-new` |
| [.claude/docs/ROADMAP.md](.claude/docs/ROADMAP.md) | Phases (outcome / scope / done when) + backlog |
| [.claude/docs/SPRINTS.md](.claude/docs/SPRINTS.md) | Sprint tasks with done criteria and user checks |
| [.claude/docs/HISTORY.md](.claude/docs/HISTORY.md) | Session snapshots — grown by `/save` |

Domain rules in [.claude/rules/](.claude/rules/) (loaded per-file by `applyTo` glob):

| File | applyTo |
|---|---|
| [.claude/rules/mcp-tools.md](.claude/rules/mcp-tools.md) | always (Unreal MCP usage) |

---

## Passive behaviors (always active)

These apply in every conversation without invoking a command.

**Before implementing any feature:**
- Check [.claude/docs/DECISIONS.md](.claude/docs/DECISIONS.md) — your design might already have an ADR.

**Before any architecture or tech decision:**
- Read [.claude/docs/ARCHITECTURE.md](.claude/docs/ARCHITECTURE.md). It is the source of truth.
- If it's a genuinely new call, add an ADR (problem / decision / rationale / alternatives).
- If your idea conflicts with an existing ADR, surface the conflict before writing code.

**When writing any comments in code:**
- Be concise with descriptions. 2 lines at most.
- Do not document abvious code. E.g. `GetWheels` is returning wheels, no need to state that.

**When writing anything external (README, docs outside `.claude/`):**
- Write as a developer, not as an AI — short, direct, lowercase after the prefix.
- Banned words: "ensure", "leverage", "facilitate", "utilize", "implement", "comprehensive", "robust", "seamless", "straightforward".
- No AI attribution markers, no "Generated with Claude", no co-author trailers.
- Commit: `fix race in HTTP shutdown` not `fixed the race condition that was occurring during HTTP server shutdown`.

---

## Available commands (Claude Code slash commands)

Live in [.claude/commands/](.claude/commands/). All are designed to be chained.

### Planning

| Command | When to use |
|---|---|
| `/plan-new` | Start of any feature or phase. Reads ARCHITECTURE.md, zeros current plan, drafts a fresh one. |
| `/plan-analyse` | After `/plan-new`. Product-engineer critique, validates against architecture. |
| `/plan-replan` | After `/plan-analyse`. Applies critique, reconciles with architecture, closes the plan. |
| `/plan-roadmap` | After `/plan-replan`. Converts final plan into ROADMAP.md + Sprint 1 tasks. |

### Execution

| Command | When to use |
|---|---|
| `/work` | Start of a coding session — sets engineer mode, ready to build. |
| `/validate` | Before starting a new phase — senior UE5 review of current state. |

### Persistence & delivery

| Command | When to use |
|---|---|
| `/save` | End of any meaningful session. Updates ARCHITECTURE, DECISIONS, HISTORY, marks tasks done. |
| `/git-commit` | After `/save`. Stages specific files, proposes commit message, pushes. |

GitHub Copilot users: these are not slash commands in Copilot, they are conventions. Reference them in chat as "follow /plan-new" and the agent executes the same sequence.

---

## Git workflow

Permanent `dev` integration branch. `main` is release-only.

- **`dev` is permanent.** All day-to-day work commits to `dev` (or a topic branch off `dev`). Never deleted, never merged away.
- **`main` is release-only.** Advances only via a long-lived PR from `dev` → `main`. `/git-pr` creates or updates that single PR.
- **Never** commit directly to `main`.
- **Never** push. We work locally.
- **Never** use `git add -A` — always stage specific files.
- Commit format: `type: short description` (max 72 chars). Types: `feat`, `fix`, `wip`, `refactor`, `chore`, `docs`, `test`.


## Coding conventions

- **No comments unless the WHY is non-obvious.** Names should explain the what.
- **One responsibility per file.** Handler files split by domain. TS tool files split by feature group.

---

## When in doubt

1. Search [.claude/docs/ARCHITECTURE.md](.claude/docs/ARCHITECTURE.md) and [.claude/docs/DECISIONS.md](.claude/docs/DECISIONS.md) first.
4. Then ask.
