# ROADMAP — River Crossing prototype (Huta → Bagnisko)

Source plans (closed):
- [plans/2026-10-25-river-crossing.md](plans/2026-10-25-river-crossing.md): the whole prototype, Testing model, Phases 0, 1, G.
- [plans/2026-10-26-mud-winch-first.md](plans/2026-10-26-mud-winch-first.md): phase reorder (mud + winch first), Phase 2 design, slope rules.
- [plans/2026-11-07-visuals.md](plans/2026-11-07-visuals.md): Phase V (mud spray rework, dirt on the jeep).
- [plans/2026-11-08-mud-lens.md](plans/2026-11-08-mud-lens.md): Phase V addition (mud on the camera lens).

Read the relevant one before starting a phase. Sprint tasks live in [SPRINTS.md](SPRINTS.md); tick them there.

**Phase order (replanned 2026-10-26):** 0 → 1 / G → **2 On foot and winch** (was 4) → **V Visuals** (added 2026-11-07) → **3 River** → **4 Damage and vegetation** (was 2) → 5 Scenario. Older docs (HISTORY.md, the 2026-10-25 plan) use the old numbers.

**Phase closes when:** the agent's rig tests pass **and** the user signs off after a playtest. If feel and numbers disagree, feel wins and the thresholds get updated.
**Performance target:** 60 fps at 1080p Medium on i7-12700H / RTX 3050 Ti Laptop (4 GB VRAM).

---

## Phase 0 — Foundation
**Status:** done (Sprint 1, closed 2026-10-26)

**Outcome.** The project runs on a low-end budget, custom code follows B1 conventions, the jeep runs on our own Chaos simulation subclass with a validated physics stepping mode, and there's a mud gym with an agent-runnable test rig.

**Scope.**
- [x] Low-end render budget (ADR-011)
- [x] `Mud*` → `B1*` rename, B1 log categories
- [x] Per-wheel debug (NetImGui + `ShowDebug B1Vehicle` + telemetry)
- [x] Template constructors → `FObjectInitializer`; `AB1Jeep`, `UB1VehicleMovementComponent`, `FB1VehicleSimulation`
- [x] Physics stepping decided (async fixed dt vs substepping), ADR-003 accepted
- [x] `AB1GymTestRig : AFunctionalTest` + headless runner
- [x] `Lvl_Gym_Mud` with layered mud patches (ADR-012)

**Done when.** The spring-to-anchor rig test is stable at 20 and 120 fps, the gym smoke test passes in PIE and headless, and the user has checked the wheel debug and the gym layout.

---

## Phase 1 — Mud
**Status:** done, signed off 2026-10-27 (Sprints 1–2)

**Outcome.** Mud that rewards driving slowly with intent: wheelspin digs you in, careful throttle gets you out, and deep mud stops you without a winch. Tires spray mud and leave trails.

**Scope (minimum).**
- [x] `UB1PhysicalMaterial_Mud`, per-wheel sinking via trace offset, traction/resistance curves, belly drag (Sprint 1)
- [x] Mud NetImGui window with live tuning (Sprint 1)
- [x] `Vehicle.State.InMud` tag, `B1Unstuck`, `ShowDebug B1Mud` (Sprint 2, needs GAS) <!-- done: 2026-10-26 -->
- [x] Niagara mud spray (GPU sprites, depth-buffer collision) (Sprint 2) <!-- done: 2026-10-27 -->
- [x] Visual tire trails with rut depth (ADR-008) (Sprint 2) <!-- done: 2026-10-26 -->

**Stretch.** Spray tinted by surface, dirt mask on the jeep (moved to Phase V), fake trail depth, character slowed in mud. Done beyond scope: mud and fuel made portable (S2-18..S2-25, MIGRATION.md).

**Done when.**
- Full throttle digs in on medium mud in 2–4 s.
- ≤ 50 % throttle crosses a 20 m medium strip.
- Deep mud stops the jeep.
- A stuck jeep creeps < 5 cm in 10 s at zero throttle.
- The user signs off at the mud playtest.

---

## Phase G — GAS + fuel
**Status:** done, signed off 2026-10-27 (Sprint 2)

**Outcome.** A single ability system on the PlayerState drives vehicle condition and fuel. The controller and game mode can handle any pawn.

**Scope (minimum).**
- [x] GameplayAbilities/Tags/Tasks modules, `AB1GameMode`, `AB1PlayerController` (any-pawn `OnPossess`), `AB1PlayerState` + ASC (ADR-004)
- [x] `UB1VehicleAttributeSet` (Condition, MaxCondition, Fuel, MaxFuel; 20 L tank), `B1GameplayTags`
- [x] Fuel consumption from engine work and mud strain, `UB1FuelConsumptionComponent` + `UB1FuelConsumptionData` (was `UB1VehicleTuningData`), batched `GE_B1FuelDrain`, out-of-fuel → no torque (ADR-010)
- [x] "Vehicle Status" NetImGui window, cheats `B1AddFuel`, `B1SetCondition`, `B1DamageVehicle`

**Stretch.** Fuel gauge in the vehicle UI. ✅ done (S2-27).

**Done when.**
- Fuel per 100 m in medium mud is ≥ 2× that on firm ground.
- At 0 fuel the jeep moves < 1 m under full throttle.
- The user signs off on the fuel pressure.

---

## Phase 2 — On foot and winch (was Phase 4)
**Status:** done (Sprints 3–4, user sign-off 2026-10-28) (plans: [2026-10-26-mud-winch-first](plans/2026-10-26-mud-winch-first.md), [2026-10-28-fallen-tree](plans/2026-10-28-fallen-tree.md))

**Outcome.** Mud + winch is proven: the jeep that can't get out of deep mud or up a muddy slope on its own gets out and up with the winch. The player can get out, carry the hook to a tree or a log, get back in and winch out.

**Scope (minimum).**
- [x] Slope rules (ADR-001 amendment): net-force standstill rule, slope hold by absolute sink depth (Sprint 3)
- [x] `Lvl_Gym_Winch`: deep strip, firm/medium/deep slopes, log lane, anchor trees (Sprints 3–4)
- [x] `UB1WinchComponent` + `UB1WinchAnchorComponent`, physics-thread tension at the winch point, counted as push in the drag caps, native analog pull input (ADR-007 amendment) (Sprint 3)
- [x] Winch debug: NetImGui "Winch", `B1Winch` telemetry, `ShowDebug B1Winch`, cheats `B1WinchAttachNearest` / `B1WinchDetach`, rig winch support (Sprint 3)
- [x] **Checkpoint:** mud + winch playtest via cheat attach, end of Sprint 3
- [x] Ability sets + input tags (ADR-014), `AB1Character`, mapping-context swap (Sprint 4)
- [x] `GA_B1EnterExitVehicle` (seat socket, engine off outside), cheats `B1ExitVehicle` / `B1EnterVehicle` (Sprint 4)
- [x] `GA_B1CarryWinchHook` with the cable as a leash, log pull via async physics tick (Sprint 4)
- [x] **Fallen tree** (replanned 2026-10-28 after the S4-09 playtest, plan [2026-10-28-fallen-tree](plans/2026-10-28-fallen-tree.md)), Sprint 4 S4-14..S4-19:
  - handbrake on pad B / Space (pad LB and the pad A brake removed), holding the jeep in place while the hook is attached; no in-car detach (ADR-007 amendment);
  - anchor areas: the cable pulls at the trunk axis; aimed hook placement with a ghost band → strap (ADR-019);
  - fallen log frozen until the cable breaks it free, then sliding and turning by physics only: no hook-position rules or scripted motion (ADR-018, user's physics-first rule);
  - banded road lane with a fallen tree in `Lvl_Gym_Winch`;
  - controls hints in the lower-left corner.

**Stretch.** Cable sag visuals, hook-in-hand mesh, animations, winch free-spool, character slowed in mud.

**Done when.**
- Flat mud suites unchanged by the slope rules.
- Firm slope climbs at 50 %; the medium slope can't be climbed unwinched at 30/50/100 %.
- On the medium slope at idle the jeep slides down slowly; on the deep slope it slides, then holds once sunk.
- Winched, the jeep climbs the medium and deep slopes and leaves the deep strip.
- Winch tension stays capped and stable at 20 and 120 fps.
- ~~A winched log moves ≥ 3 m toward the jeep.~~ Replaced 2026-10-28 by the fallen tree:
  - ramming the tree, hooked or not, moves it ≤ 1 cm;
  - hooked near its free end with the handbrake held, it swings open a ≥ 3 m gap about its root end while the jeep moves ≤ 0.5 m, and the jeep drives through;
  - hooked at the middle, it opens less than hooked at the end (probably not passable; that's the puzzle);
  - with the handbrake held and the hook on a tree, the jeep moves ≤ 0.3 m at full pull, and a detach mid-hold doesn't shove it.
- The user signs off at the Sprint 3 checkpoint (mud + winch) and at the phase playtest (full loop on foot, including the fallen tree scenario from VISION.md).

---

## Phase V — Visuals
**Status:** done (Sprint 5, signed off 2026-11-08) (plans: [2026-11-07-visuals](plans/2026-11-07-visuals.md), [2026-11-08-mud-lens](plans/2026-11-08-mud-lens.md); ADR-021, ADR-022)

**Outcome.** Mud reads as mud. The tires throw a dense spray of small clumps of mixed sizes with a faint haze, when spinning and when rolling (`Concept/mud-car-drive.mp4`). The Wrangler starts clean (`Concept/jeep-wrangler.png`) and collects mud in stages (tires, then arches and sills, then splatter on the doors and bumpers, then spots on the hood, roof and glass), capped at `Concept/jeep_wrangler_mud_reference.jpg`. Hard wheelspin splashes the chase camera's lens; the mud builds up and fades, capped at `Concept/mud-on-the-screen.jpg` (added 2026-11-08).

**Scope (minimum).**
- [x] Spikes: CPD and tags on the inherited Wrangler components, Niagara mesh renderer + depth-buffer collision through MCP, body-space arch centres (Sprint 5)
- [x] `Scripts/MudTextures` generator (masks + clump mesh), `MF_B1MudDirt` (local-space triplanar, region-weighted reveal) in the Wrangler materials, including a Wrangler glass material (Sprint 5)
- [x] `UB1MudDirtComponent` (portable): `B1MudDirt`-tagged targets matched to wheels, CPD writes, `PreviewDirt`, accumulation from `B1MudThrow` in seconds-to-full, capped and monotonic (ADR-021) (Sprint 5)
- [x] Photo booth stills at 5 dirt levels from the two concept angles; **checkpoint 1** (user signs off the look before the accumulation is built) (Sprint 5)
- [x] Dirt debug: Mud window section, `B1Dirt` cheat, telemetry, `ShowDebug B1Mud`; rig `StartDirt` + dirt metrics and four dirt rigs (Sprint 5)
- [x] `NS_B1MudSpray` rebuilt as clumps (mesh) + droplets + haze, rate from `B1MudThrow` (rolling spray), `User.Intensity`, Spray section in the Mud window (Sprint 5)
- [x] **Mud on the lens** (ADR-022, Sprint 5 S5-21..S5-28): portable `UB1MudLensComponent` (exposure from the spray's launch speed, wide aim and distance; 4 splash slots with hold + fade; `LensSecondsToFull`), `M_B1LensMud` post-process on the `B1MudLens`-tagged chase camera, generated drop/speck textures, preview-volume stills + **lens checkpoint**, Lens section / cheats / telemetry, four lens rigs

**Stretch.** Fresh vs dried mud (`Wetness`, CPD slot 5); spray spawned along the tread arc; lens drops running down as they age; lens splashes centred on the throwing wheel.

**Out of scope.** The `mud-car-splash.mp4` sheet splash, washing dirt off (Phase 3 can add it), UV-painted dirt, the dirt material on the buggy, performance work. Lens: particle-to-lens hits, body occlusion of the throw, the in-cabin camera, wipers, water on the lens, other vehicles' spray.

**Done when.**
- On firm ground the jeep stays clean (`B1Dirt_Firm`: all 0).
- In medium mud the tires lead the body (`B1Dirt_Medium`).
- 90 s at 50 % in medium mud gives body ≥ 0.9 and wheels at 1 (`B1Dirt_Drive90`); dirt holds at exactly 1 once capped (`B1Dirt_Cap`).
- Lens: firm driving never splashes (`B1Lens_Firm`), rolling through medium stays below wheelspin (`B1Lens_Roll`), wheelspin in medium splashes (`B1Lens_Spin`: ≥ 3 splashes, peak ≥ 0.3, capped at 1), and the lens clears within 10 s (`B1Lens_Fade`).
- Existing suites unchanged (core, mud, winch).
- The user signs off the stills (checkpoint 1), the lens look (lens checkpoint) and the Swamp playtest (checkpoint 2).

---

## Phase 3 — River
**Status:** not started (replan before starting)

**Outcome.** The deep ford is a fight against the current; the shallow ford is a mud trap.

**Scope (minimum).** Box `AB1RiverVolume`, hull-point side force, traction loss with submersion, deep and shallow-mud ford in `Lvl_Gym_River`, flow debug.
**Stretch.** Spline river, splash VFX, water surface shading.

**Done when.**
- ≥ 8 of 10 unwinched rig crossings of the deep ford end downstream on the shallow side.
- Winched to a tree, the jeep crosses the deep ford (moved from the winch phase).
- The user signs off.

---

## Phase 4 — Damage and vegetation (was Phase 2)
**Status:** not started (replan before starting)

**Outcome.** Speed has a cost: bumps, trees and branches damage the jeep, and vegetation reacts to it. Branches can be cleared with a machete.

**Scope (minimum).** Impact damage from chassis hits, condition penalties, large tree (also a winch anchor), small tree (topples), bush (drag + scratch), breakable branch, `GA_B1Machete` (moved from the winch phase, uses the ADR-014 ability plumbing), `ShowDebug B1Obstacles`. The physics log moved to Phase 2. Ramming the fallen tree (Phase 2) costs condition.
**Stretch.** Bush/branch WPO bending, break VFX, hit sounds.

**Done when.**
- The branch section at 40 km/h costs ≥ 3× the condition lost at 15 km/h.
- At < 30 % condition, 0–30 km/h takes ≥ 25 % longer.
- Cut branches cost 0 condition.
- The user signs off.

---

## Phase 5 — Scenario level
**Status:** not started (replan before starting)

**Outcome.** The full Huta → Bagnisko situation from VISION.md is playable.

**Scope (minimum).** Greybox route: Huta → collapsed bridge → forest path (mud, a muddy uphill, bumps, branches, a log) → ford with two routes → Bagnisko arrival trigger. Built from modular meshes + spline tools. The user sculpts the landscape and paints any foliage; the agent places set pieces, volumes, anchors and triggers via MCP.
**Stretch.** Vegetation dressing, landmarks.

**Done when.** The route is playable start to finish with both ford routes, at 60 fps on the target laptop, and the user signs off.

---

## Backlog

Bugs and ideas found during `/work` that are out of the current task's scope.

- _(empty)_
