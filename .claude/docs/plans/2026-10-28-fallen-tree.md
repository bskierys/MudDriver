# PLAN — Fallen tree (S4-09 playtest replan)

Replans the winchable log after the user's S4-09 check (2026-10-28). It stays inside Phase 2 and Sprint 4: tasks S4-14..S4-19 come before the phase playtest (S4-10). Scenario: VISION.md → "Fallen tree". First drafted and converted on 2026-10-28 (archived at [plans/2026-10-28-fallen-tree.md](2026-10-28-fallen-tree.md)). Reopened for critique round 1 and revision 1 below, then re-archived here by `/plan-roadmap`.

## Problem

S4-09 built a free-simulating 1000 kg cylinder with the anchor at one end cap. In the playtest:
- driving into or over the log rolls it around. It should stay still;
- the cable hooks to the end of the log, which looks wrong. The cable should pull from the middle of the trunk;
- reeling a hooked log pulls the jeep toward it. There's no way to hold the jeep in place.

The user's target situation (VISION.md, "Fallen tree"): a tree lies across a road closed off with bands. Ramming doesn't move it. The player hooks it near one end, holds the handbrake and winches. The tree swings open like a door, and the player drives through the gap. On top of that:
- free hook placement over an area of the trunk, previewed by a yellow ghost band that turns into a black strap on attach (The Alters building placement);
- a handbrake on pad B / Space;
- controls hints in the lower-left corner;
- no in-car detach. Detaching is done on foot.

**Physics first (user, 2026-10-28).** The tree moves by physics, not by rules or scripts. Hooked at the middle, it probably won't open enough to pass. That's expected: the player has to work out where to hook so the physics works for them. It may not be perfect, but the player struggling to pull it off beats a fake that always succeeds.

## Who it's for

The user as player: the first obstacle you clear with the winch instead of getting yourself out with it. The answer to "how do I get past this" should come from where you hook and how you brace the jeep.

## Success criterion

On a banded road in `Lvl_Gym_Winch`, ramming the fallen tree doesn't move it. Hooked near the free end with the handbrake held, the winch swings it open wide enough to drive through, while the jeep stays put. Hooked at the middle, it opens less (probably not enough), and nothing in the code makes that happen except physics.

## Design decisions

| Topic | Decision | ADR |
|---|---|---|
| Log physics | **Frozen until pulled.** At rest the log doesn't simulate (a kinematic body: the jeep hits a wall). Once cable tension passes `BreakawayTension`, it simulates and moves by Chaos alone: slides and turns on its collision, with a heavy root end. After it has stopped and tension is below the threshold for `RefreezeDelay`, it freezes again. User picked this over a stump hinge, an always-simulated heavy log and our own planar solver. | ADR-018 (proposed) |
| Log body | **Unscaled simulating box as the root.** It's the flat-sided no-roll collision and gives a clean body frame (the S4-09 scale bug can't come back). The round trunk visual is a child with no collision, scaled freely. The root ball is a welded child primitive. The compound mass puts the COM toward the root, so no `COMNudge`. The anchor attaches to the box root. | ADR-018 |
| Where to hook | **Physics only.** No hook-position rules, no scripted motion, no forces that depend on where you hooked. With friction along the trunk, turning about the root end takes about half the force at the tip that it takes at the middle. So near the tip the 60 kN cap can swing it, and at the middle it probably can't. That's the intended puzzle. | ADR-018 |
| Hook placement | `UB1WinchAnchorComponent` becomes an **area**: a segment on the trunk axis plus the trunk radius. `UB1HookAimComponent` on the character aims a point on it (camera ray, clamped to the area) and shows a ghost band. It **stores the aim on the carried winch** (`SetAimTarget(Anchor, Alpha)`), so the "Attach" option, `GA_B1AttachHook` and the debug views all read it through `FindCarriedBy`, the hook state's one owner (ADR-007). Interact attaches there, and the band turns into the solid strap. Fixed hook points go away. | ADR-019 (proposed) |
| Force point | The cable pulls at the strap's point **on the trunk axis**, so a side hook adds no roll torque. The visual cable ends on the strap's surface, on the side facing the winch. The cable's particle sweeps also block PhysicsBody, so a cable lying across the log rests on it. | ADR-019 |
| Handbrake | `IA_Handbrake`: Space (already bound) and **pad B**. The template's pad LB binding is removed (user). A normal Chaos handbrake while driving. **With the hook attached**, the held handbrake adds a standstill hold in `FB1VehicleSimulation`, up to `HandbrakeHoldForce`. The hold is computed from **this step's** winch force and drops in the step the anchor clears. Not scaled on mud (user: see how it plays). | ADR-007 amendment |
| Brake on pad | Pad A no longer brakes (user); LT stays brake/reverse. That way the hints read RT / LT / B. | — |
| Detach | In the jeep: nothing. `IA_B1WinchDetach` and its Q / pad Y bindings go away; detaching happens at the anchor (S4-07's "Detach" option). The cheat and the Winch window's button stay as debug (user). Supersedes the S4-13 detach key. | ADR-007 amendment |
| Controls hints | A code-only widget in the lower-left corner lists the actions available now. Entries come from a data asset: input action, label, required and blocked tags. It shows both keys ("RT / W  Drive"), like the interaction prompt (user). New loose tag `Vehicle.State.WinchHooked` gates the winch hint. | — |

### Log model in detail (ADR-018)

- **States.** Frozen (no simulation, blocks everything) ↔ free (simulating).
  - The log's game-thread tick reads the hooked winch's published `Tension`. Frozen → free when tension ≥ `BreakawayTension`.
  - Free → frozen when tension < `BreakawayTension`, linear speed < 5 cm/s and angular speed < 2 °/s, all for `RefreezeDelay` (0.5 s).
  - The switch lags one frame behind the tension, which is fine for a threshold. The cable force on a free log stays in the vehicle sim's same-step pass (ADR-007, S4-09).
- **Winch side, already in place.** The winch sends `AnchorLocation` every tick, and `GetSimulatingBody` checks the attach parent's `IsSimulatingPhysics` per call. So a frozen log is a static anchor at its current pose, and freeing it only adds the proxy to the next input. No `AttachSerial` bump, and the rest length is kept.
- **Body.**
  - Root: unscaled `UBoxComponent` (simulating, PhysicsActor) sized to the trunk.
  - Trunk visual: cylinder mesh, no collision.
  - Root ball: welded child with collision and its own mass.
  - Material: high-friction `PM_B1Log` (the user creates it by hand).
- **Breakaway gate.** It exists only so ramming can't move a resting log (user's pick). Its lower bound is measured, not guessed: the tension a braked jeep reaches while being dragged toward a frozen log. `BreakawayTension` ≥ 1.2 × that, so the handbrake is needed to free the log. Its upper bound is the cap.
- **Expected physics.** These are targets for the physical knobs only: mass, friction, root ball mass and size.
  - With uniform friction turning about the root end, the tip force needed is about μ·m·g/2 and the middle force about μ·m·g.
  - The root has to push back with about the pull force to stay the pivot. Otherwise a free rod turns about a point roughly 0.3 L from the far end.
  - Tuning target: tip resistance about 0.55–0.8 × cap. The rig measures where the pivot is (`logPivot=`).
  - If the root doesn't hold the pivot, first try a heavier or wider root ball. An authored root-end drag force only goes in with the user's OK (physics first).
- **Ramming.** Frozen, ramming moves it by 0. While the winch pulls it free, the jeep can shove it. Accepted.
- **Debug.**
  - The log draws its own state (frozen/free, tension vs breakaway, switch count, yaw) whatever the viewer's winch is: `ShowDebug B1Log`.
  - Every switch is logged on `LogB1Winch` with the tension, so headless runs explain a failed rig.

### Handbrake hold in detail (ADR-007 amendment)

- Active when the handbrake input is on, the hook is attached, ≥ 3 wheels touch the ground, and ground speed < `HandbrakeHoldSpeed` (30 cm/s).
- Hold = −(this step's winch force + gravity along the ground), projected onto the ground plane, plus a drift-killing velocity term. Capped at `HandbrakeHoldForce` (`Winch|Config`, default 80 kN), applied at the COM. It's static-friction-like: it never pushes.
- It's computed right after `UpdateWinch` in the same step, never from a cached force. It's zero in the step the anchor clears, so a detach or a sudden drop in tension can't leave a phantom shove.
- The mud sim gets winch + hold as one external force (`SetExternalForce`), so the standstill rules don't see push that isn't there.
- Not scaled on mud (user).

## Scope

Sprint 4, section "Playtest feedback (S4-09, 2026-10-28)". Order: S4-14 → S4-15 → S4-16 / S4-17 → S4-18 → S4-19 → S4-10. S4-15 and S4-17 can run in parallel.

1. **S4-14 Handbrake + winch hold, no in-car detach** (3 h).
   - Input: `IA_Handbrake` on Space + pad B, the LB binding removed, the pad A brake removed.
   - Same-step hold, per the model above.
   - `IA_B1WinchDetach` and `WinchDetachAction` removed.
   - Rigs: `B1Winch_HandbrakeHold`, `B1Winch_HandbrakeDetach`.
2. **S4-15 Anchor areas + pull at the trunk axis** (3 h).
   - Area segment and radius on `UB1WinchAnchorComponent`, and `AttachTo(Anchor, AreaAlpha)`.
   - Force point on the axis; cable end on the strap surface; static strap mesh at the hook point.
   - The cable blocks PhysicsBody.
   - Trees and the log get areas. `StartAnchorAlpha`, the cheat, the rig `WinchAnchorAlpha` and the debug take an alpha.
3. **S4-16 Hook placement ghost** (3.5 h).
   - `UB1HookAimComponent` aims and writes `SetAimTarget` on the carried winch; yellow translucent band.
   - Interact attaches at the aimed point.
   - Check whether MCP `MaterialTools` can create the translucent material; ask the user if not.
   - Rig: `LookAt` step and `B1OnFoot_HookAim`.
4. **S4-17 Fallen log: frozen until pulled** (4 h).
   - Body restructure (box root, visual child, welded root ball).
   - States and the breakaway gate.
   - The user creates `PM_B1Log`.
   - `ShowDebug B1Log`, switch logging, Winch-window log section, `B1LogReset`.
   - Rigs: `B1Log_Ram`, `B1Log_RamHooked`.
5. **S4-18 Road lane + fallen tree tuning** (4 h).
   - Banded road with a ~10 m tree.
   - Measure the breakaway lower bound first.
   - Tune the physical knobs toward the expected-physics targets; metrics `logGap`, `logPivot`, drift per second.
   - Rigs: `B1Log_PullEnd`, `B1Log_PullMiddle`, `B1Log_PullNoHandbrake`, `B1Log_DriveThrough`.
6. **S4-19 Controls hints** (2.5 h). Data asset + code-only widget, shared key-text helper with the prompt, `Vehicle.State.WinchHooked`.
7. **S4-10 Phase 2 playtest** (updated): the full fallen tree scenario is added to the loop.

### Out of scope
- Damage from ramming the tree (Phase 4, `UB1ObstacleComponent` / impact damage).
- Making the log immune to the jeep while the winch pulls it free.
- Any hook-position rule or scripted log motion (user: physics first). A stump hinge, our own planar log solver, cutting the tree.
- Hook-in-hand mesh, hook or strap animations, a styled UI. The strap is a basic shape with a material.
- Showing keys by last-used device (both are shown, user).
- Several hooks or snatch blocks.

## Approach

All on the existing winch path (ADR-007). The opposite force on a free log stays in `FB1VehicleSimulation::ApplyDeferredForces`. A frozen log is a static anchor, which the per-tick `AnchorLocation` and `GetSimulatingBody` already handle. The log decides its own state on the game thread from the published tension, so there's no new physics-thread state mutation. It's rebuilt around an unscaled box body, so every child and the area live in a clean frame.

The hold sits next to the winch force in the vehicle sim, is computed from the same step's force, and goes through the same `SetExternalForce` hook. It's configured on the winch, since it only runs with a hook attached.

The area replaces the anchor's point everywhere the anchor location was read: option point, `IsInReach`, sim input, cable end and debug. The aim component only computes; the result lives on the carried winch, so the prompt, the attach and the debug agree (ADR-017).

The hints widget follows the prompt's pattern: a code-only `UUserWidget` with keys from `QueryKeysMappedToAction`. The cleaned-up vehicle mapping is what makes it read RT / LT / B.

Tuning only touches physical parameters. Where a rig shows the physics falling short, the fix is a better physical setup or the user's call, not a scripted assist.

## Validation

Agent, headless, in `Lvl_Gym_Winch`. Thresholds marked *measured* are set from the first run, then held.
- `B1Winch_HandbrakeHold`: braked + handbrake, hooked to `Tree_FirmPull`, full pull 10 s → jeep `lastDisp` ≤ 0.3 m; tension reaches the stall (≥ 95 % of the cap). The existing tree rigs (no handbrake) are unchanged within 1 kN.
- `B1Winch_HandbrakeDetach`: the same, then detach by step while holding at the cap → jeep `lastDisp` ≤ 5 cm over the next 1 s (no phantom shove).
- `B1Log_Ram`, `B1Log_RamHooked` (hooked, no pull): full throttle into the log from 15 m for 5 s → log moves ≤ 1 cm, `logSwitches` = 0.
- `B1Log_PullNoHandbrake`: hooked at the tip, braked only → the jeep is dragged toward the log and `logSwitches` = 0. The tension it reached sets the breakaway lower bound.
- `B1Log_PullEnd`: hooked at alpha 0.9 (free end), braked + handbrake, full pull → `logGap` ≥ 3 m within *measured* s (target ≤ 60 s); `logPivot` within 1 m of the root end; jeep `lastDisp` ≤ 0.5 m; `logState` = frozen 2 s after the pull stops.
- `B1Log_PullMiddle`: alpha 0.5, same script → `logGap` smaller than in `PullEnd`, reported as is. The rig fails only if middle opens as much as end, which would mean hook position doesn't matter. Drift per second is reported, to catch Chaos creep.
- `B1Log_DriveThrough`: after the `PullEnd` script, detach, then steer through the gap → the jeep passes 5 m beyond the log line.
- `B1OnFoot_HookAim`: look at 1.2 m on `Tree_FirmPull` and interact → hook point within 10 cm of it. Look at 3 m → clamped to the area's top (1.6 m).
- All suites (core, mud, winch) still pass.

`logGap` = the shortest distance, measured across the road, from the free-side band line to the log's footprint (box root corners + root ball), sampled across the road's width at the log's position.

User (S4-10): the full fallen tree scenario on the road; hooking at the middle struggles, near the end works; the ghost band is easy to place; the handbrake holds; the hints are right in the jeep and on foot. Feel wins over the numbers.

## Risks

1. **The door doesn't emerge.** If the root end doesn't hold the pivot, the log slides and spins instead of swinging. Mitigation: `logPivot` in the rig, then a heavier or wider root ball. An authored root-end drag force only with the user's OK.
2. **A free log creeps under a force below friction.** Hooked at the middle, the log is free while tension stays ≥ breakaway, so it relies on Chaos static friction under a steady 60 kN. Mitigation: `PullMiddle` reports drift per second. Some creep is acceptable physics. If it looks wrong, try the material and the contact shape first; an authored standstill term only with the user's OK.
3. **A phantom shove from the hold.** An 80 kN hold built from a stale winch force shoves the jeep when tension drops. Mitigation: same-step computation, zero on anchor clear, `B1Winch_HandbrakeDetach`.
4. **Freeze / free flapping** at the threshold. Mitigation: hysteresis (free at breakaway; freeze only below it after `RefreezeDelay` at rest). The switch count is logged and is a rig metric.
5. **Toggling simulation on a body the winch sim reads.** The proxy might change on `SetSimulatePhysics`. `GetSimulatingBody` is read every tick, and the sim only uses the handle sent with that step's input (cleared after use since S4-09). 🔍 S4-17 checks that the pose and velocity survive the toggle.
6. **Handbrake hold vs mud rules.** Mitigation: winch + hold reach the mud sim as one external force; the mud suite (flat, no winch) must stay unchanged.
7. **Assets the MCP tools can't create.** `PM_B1Log`: the user creates it in S4-17. The translucent band material: check `MaterialTools` in S4-16, and ask the user if it can't.

## Dependencies

- S4-07 (hook carry), S4-08 (prompt), S4-09 (in-step anchor force), S4-11 (interaction scan), S4-13 (winch input). All done.
- User: `PM_B1Log` in S4-17; the translucent parent material in S4-16 only if MCP can't create it.

---

## Critique (round 1) — 2026-10-28 — Fallen tree

Subject: [plans/2026-10-28-fallen-tree.md](2026-10-28-fallen-tree.md) and its tasks S4-14..S4-19 in [SPRINTS.md](../SPRINTS.md). It was converted straight to tasks, so this reviews both against ARCHITECTURE.md, ADR-007, ADR-017, ADR-018 and ADR-019. User decisions already recorded: the hold isn't scaled on mud; the user creates `PM_B1Log` by hand in S4-17; for the band material, the agent checks MCP first and asks if it can't.

### Architectural alignment — 4/5
The plan sits on the existing winch path and needs less new plumbing than it says. `UB1WinchComponent` already sends `AnchorLocation` every tick (`B1WinchComponent.cpp:70`), and `GetSimulatingBody` checks the attach parent's `IsSimulatingPhysics` per call (`B1WinchAnchorComponent.cpp:35`). So a frozen log is a static anchor for free, and freeing it only adds the proxy to the next input, with no serial bump. Keeping the state switch on the game thread is right. Two structural problems:
- **The log body inherits the S4-09 scale trap.** `AB1WinchLog`'s root is the scaled trunk (actor scale 0.5 × 0.5 × 5). Every child added in S4-17 (root ball, flat-sided collision, strap) inherits that scale, so a root ball sphere becomes a 5 m spindle. The area segment would also live in a scaled frame, the class of bug S4-09 just fixed. Use an **unscaled simulating box as the root** instead: it's the no-roll collision and has a clean body frame. Put the round trunk visual under it (no collision, scaled freely) and weld the root ball under it as a child primitive. The compound mass then shifts the COM toward the root on its own, so `COMNudge` isn't needed. The anchor attaches to the box root (`GetSimulatingBody` reads the attach parent).
- **Aim state has no single owner.** ADR-007 (S4-07) says hook state has one owner: the winch, found through `FindCarriedBy`. ADR-019 puts the aimed anchor and alpha on a character component, which the anchor's "Attach" option, `GA_B1AttachHook` and the debug views would then each read. Let `UB1HookAimComponent` compute the aim, but store the result on the carried winch (`SetAimTarget(Anchor, Alpha)`), so all readers go through `FindCarriedBy` as today.

The hold sits next to the winch force in the project sim, outside the portable mud code. Right.

### Scope clarity — 4/5
The out-of-scope list is sharp, and the user decisions close the open ends. One scope hole is now confirmed in the asset. `IMC_Vehicle_Default` (read over MCP):
- Space is already `IA_Handbrake`;
- the pad handbrake is **LB**, not B;
- pad **A** is bound to `IA_Brake` next to LT;
- pad B is free.

S4-14 only says "add pad B". S4-19 shows the first pad key per action, so the hints would read "A / S  Reverse" and "LB / Space  Handbrake", not the user's RT / LT / B. The task has to decide what happens to the LB handbrake and the A brake. Removing them is cleanest, since the hints then match the user's list with no override mechanism. The VR keys in the template IMC are noise but harmless.

The second hole is visual. The cable's particle sweeps block WorldStatic, WorldDynamic and mud (ADR-007, S4-12), but the log is PhysicsBody, so a cable lying across a log passes through it. Adding PhysicsBody to the cable's block list is safe: vehicles and pawns stay ignored.

### Validation specificity — 3/5
The rigs exist and cover the right cases, but three targets don't hold up on the physics:
- **The middle-vs-end ratio is fixed at about 2.** With uniform trunk friction turning about the root end, the tip force needed is μ·m·g/2 and the middle force is μ·m·g. Materials and mass scale both together. So "middle moves < 0.5 m" and "end swings" can both hold only if the tip resistance sits at about 0.55–0.8 of the 60 kN cap (middle at 1.1–1.6 × cap). S4-18 should tune toward that number, not toward "a few degrees per second".
- **The root end must resist about the tip force.** Turning about the root end means the root supplies a reaction about equal to the pull: roughly 35–45 kN at the target above. Without that, a free rod pulled at one end turns about a point about 0.3 L from the far end, not about the root. The door then becomes a sliding, spinning log. A root ball sized by mass alone needs several tonnes. The rig should measure where the pivot is, and the fallback "log-side ground drag" should be planned as root-end drag in the free state. The plan's risk 3 names it only generically.
- **`logGap` is undefined.** "Clear width between the log and the free-side band along the road" can be read three ways once the log has turned. Define it as the shortest distance, measured across the road, from the free-side band line to the log's footprint, sampled over the road's width at the log's position. Better still, add the real criterion: `B1Log_DriveThrough` (after `B1Log_PullEnd`, detach by step, steer through: the jeep passes 5 m beyond the log line).

`B1Log_PullNoHandbrake` ("logSwitches = 0") needs `BreakawayTension` above the tension a braked jeep reaches while being dragged. Nobody has measured that. S4-09's braked jeep held 17 kN at 0.42 m of travel, and tree FirmPull peaks at 36 kN. Measure it first; that number is the lower bound for breakaway.

### Risk realism — 3/5
Risks 1 (flapping), 2 (proxy on toggle) and 4–6 are real but easy. The actual top three:
1. **The door doesn't emerge** (pivot location, see above). It decides whether the scenario works at all. Mitigation: measure the pivot in the rig (`logPivot=`, distance from the root end); add root-end drag in the free state if the pivot is more than 1 m off.
2. **A free log creeps under a force below friction.** Hooked at the middle, the log breaks free (tension reaches the cap) but should stay put. It stays free as long as tension ≥ breakaway, so for 30 s it relies on Chaos static friction under a steady 60 kN on a box contact. Any creep breaks the "< 0.5 m" check and looks wrong. Mitigation: `B1Log_PullMiddle` reports drift per second. If it creeps, add a standstill term for the free log in `ApplyDeferredForces`, the same idea as the jeep hold.
3. **The hold must follow the winch force in the same step.** The hold cancels up to 80 kN. If it ever uses a cached or one-step-old winch force, a sudden drop (log breaks free and accelerates, detach, stall release) leaves a phantom 60 kN shove on a 2 t jeep for a step. Mitigation: compute the hold from this step's `UpdateWinch` result and switch it off in the step the anchor clears. Add a rig that detaches mid-hold at the cap: the jeep must not lurch (`lastDisp` ≤ 5 cm over 1 s).

### Agent ergonomics — 4/5
The rig additions chain well: `WinchAnchorAlpha`, `LookAt`, `logState`, `logMove`, `logYaw`, `logSwitches`. Gaps:
- `ShowDebug B1Winch` only draws the viewer's own winch, so the unhooked ram test shows nothing about the log. The log should draw its own state (frozen/free, tension vs breakaway, switches) under the same category or a new `ShowDebug B1Log`.
- Each state switch should be logged on `LogB1Winch` with the tension, so headless logs explain a failed rig.
- `MaterialTools` is in the MCP toolset list, so S4-16's research is probably a quick yes.

### Implementability — 4/5
About 19 h fits the sprint for one developer, and S4-15 / S4-17 can run in parallel after S4-14.
- S4-17 at 4 h is tight with the body restructure, the toggle research, the user's material hand-off, debug and two rigs.
- S4-18 at 3 h is optimistic for a three-way tuning window (breakaway lower bound, tip < cap, middle > cap, root holding). Make it 4 h, with the root-end drag as a planned step.
- Nothing needs to split out of the sprint.

### Recommended changes
1. **Log body (S4-17, ADR-018):** an unscaled simulating box root (no-roll collision, clean body frame), the trunk visual as a child with no collision, the root ball welded as a child primitive, the anchor on the box root. Drop `COMNudge`.
2. **Aim result on the winch (S4-16, ADR-019):** `UB1HookAimComponent` writes `SetAimTarget(Anchor, Alpha)` on the carried winch; the option, the ability and the debug read it through `FindCarriedBy`.
3. **Input layout (S4-14):** pad B + Space on `IA_Handbrake`; remove the LB handbrake and the pad A brake from `IMC_Vehicle_Default` (confirm at the user check), so the hints read RT / LT / B.
4. **Tuning targets (S4-18):** tip resistance about 0.55–0.8 × cap (middle then above the cap, ratio about 2). Measure where the pivot is (`logPivot=`). Root-end ground drag in the free state is a planned step, used if the pivot is more than 1 m from the root end. Estimate 3 → 4 h.
5. **Breakaway lower bound (S4-18):** measure the tension of a braked jeep dragged toward a frozen log first, then set `BreakawayTension` ≥ 1.2 × that.
6. **Free-state creep (S4-18):** `B1Log_PullMiddle` reports drift per second. If it creeps, add a standstill term for the free log in `ApplyDeferredForces`. Add it to risks.
7. **Hold from the same step (S4-14, ADR-007):** state that the hold uses this step's winch force and drops with the anchor. Rig: detach mid-hold at the cap, `lastDisp` ≤ 5 cm over 1 s.
8. **Define `logGap`, add `B1Log_DriveThrough` (S4-18):** shortest distance across the road from the free-side band to the log's footprint; the drive-through rig checks the real outcome.
9. **Log debug on its own (S4-17):** the log draws its state regardless of the viewer's winch and logs each switch with the tension on `LogB1Winch`.
10. **Cable over the log (S4-15):** add PhysicsBody to the cable's block responses so a cable across the log rests on it.

### Verdict
- [ ] Ready to convert to roadmap (run `/plan-roadmap`)
- [x] Needs revision (run `/plan-replan`)

---

## Revision 1 — 2026-10-28

User decisions taken in with this revision:
- Pad handbrake moves from LB to B, and pad A is removed from the vehicle mapping.
- **Physics first:** hooking at the middle will probably not let the player pass, and that's expected. The player has to find where to hook. Physics must not be faked; a struggle beats a scripted success.

### Changes applied from critique
- 1, log body: unscaled box root, visual child with no collision, welded root ball, anchor on the root; `COMNudge` dropped (Design decisions, Log model, ADR-018).
- 2, aim result on the winch: `SetAimTarget(Anchor, Alpha)` on the carried winch, read through `FindCarriedBy` (Design decisions, ADR-019).
- 3, input layout: `IA_Handbrake` on Space + pad B, LB removed, pad A brake removed (user).
- 4, tuning targets, partly applied: tip about 0.55–0.8 × cap, ratio about 2, `logPivot` measured, S4-18 3 → 4 h. Root-end drag is deferred, see below.
- 5, breakaway lower bound measured from `B1Log_PullNoHandbrake`, `BreakawayTension` ≥ 1.2 × that.
- 6, free-state creep, partly applied: `PullMiddle` reports drift per second; added as risk 2. The standstill term is deferred, see below.
- 7, same-step hold, zero on anchor clear, rig `B1Winch_HandbrakeDetach` (Handbrake hold, risk 3, ADR-007 amendment).
- 8, `logGap` defined; `B1Log_DriveThrough` added.
- 9, `ShowDebug B1Log` and switch logging with tension on `LogB1Winch`.
- 10, the cable blocks PhysicsBody (S4-15).
- Validation reworked around physics first. `PullMiddle` no longer has a "< 0.5 m" pass mark; it only has to open less than `PullEnd`, and its gap is reported. The `PullEnd` time is measured (target ≤ 60 s). Feel wins at S4-10.

### Changes deferred (with reason)
- **Root-end ground drag (from 4) and the free-log standstill term (from 6):** both are authored forces standing in for physics, which goes against the user's physics-first decision. They're no longer planned steps. S4-18 first tries physical setups (root ball mass and size, material, contact shape). Either force only goes in if the rigs show the physics can't produce a door, and only with the user's OK.

### ADR notes
- No conflict with an accepted ADR. ADR-018 and ADR-019 are proposed, from this plan's first draft, and are updated in place (body structure, physics-first tuning, aim result on the winch). The ADR-007 amendment (proposed) gains the same-step hold rule and the input layout.

### Verdict
- [x] Ready to convert to roadmap
