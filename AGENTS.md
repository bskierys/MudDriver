# AGENTS.md

Universal entry point for AI coding agents (Cursor, Aider, Continue, Cline, Windsurf, anything that reads `AGENTS.md`).

**The full instruction set lives in [CLAUDE.md](CLAUDE.md). Read it.**

This file exists so that agents which look for `AGENTS.md` by convention land at the same harness used by Claude Code and GitHub Copilot.

## What this project is

`MudDriver` is a prototype of the driving and environment system in Unreal Engine 5.
For full overview of the game prototype, go to `VISION.md`.

## What you need to know to start work

1. Read [CLAUDE.md](CLAUDE.md) — full conventions, tech stack, doc index.
2. Read [VISION.md](VISION.md) — The vision for the game prototype. Read to always have the idea what is the final goal.
3. Domain rules in [.claude/rules/](.claude/rules/) rules, conventions.
5. Skills (loadable domain knowledge) in [.claude/skills/](.claude/skills/).

## Hard rules

- After C++ changes: run UnrealBuildTool, fix errors, then report done.
- Never commit directly to main. Work goes on dev or a topic branch off dev.
- We work locally, never push.