# PLAN — Mud + winch first (phase reorder after Phase 1/G)

Replans the phases after Sprint 2 of the closed [River Crossing plan](plans/2026-10-25-river-crossing.md). Sprint 2 (Phase 1 polish + Phase G) is **not** touched.

## Problem

The roadmap order after Phase 1/G is Damage and vegetation → River → On foot and winch. The user's priority is to prove mud + winch first: mud is done and signed off, and the winch is the tool that turns mud from "stuck" into a puzzle. River comes second, damage and vegetation last.

VISION.md now has a new mud + winch situation, the **muddy slope** (added 2026-10-26): a slope the jeep climbs easily on firm ground but can't climb on mud. Wheelspin digs in, grip goes, the jeep slides back. The winch, anchored to a tree at the top, gets it up.

Today the mud model only knows flat ground. Its standstill rules (`B1VehicleSimulation.cpp:229-235`, `:293-296`) treat the tire force as "the engine's push". On a slope that force is mostly the tire holding the jeep against gravity, so the rules would push the jeep downhill at idle and brake a slide hard. Neither was designed.

The current Phase 4 also carries items that depend on later phases:
- the machete needs breakable branches (`UB1ObstacleComponent`, Phase 2);
- "crosses the deep ford" in its Done when needs the river (Phase 3);
- "physics log" sits in Phase 2's scope, but its only use is as a winch target.

## Who it's for

The user as player and designer: the first hands-on proof that the core loop (drive → get stuck → get out → winch → drive on) feels good, before spending time on the river or damage.

## Success criterion

In `Lvl_Gym_Winch`, the jeep that can't get out of deep mud or up the muddy slope on its own gets out and up with the winch, and the user signs off on the feel. The mud + winch part is proven at the end of Sprint 3 (attach by cheat, pull from the car), before any on-foot work.

## Design decisions

| Topic | Decision | ADR |
|---|---|---|
| Mud on slopes | **Hold by sink depth, not by preset.** A barely sunk jeep slides down slowly (slippery mud, gravity wins). A deeply sunk jeep is held by the mud (no slide) but still can't climb. A jeep on Deep slides until it has sunk in, then stops. | ADR-001 amendment (2026-10-26) |
| Standstill rules | Rolling resistance and belly drag judge "push" from the **net** force along the ground: tire force + gravity along the ground + winch pull. | ADR-001 amendment (2026-10-26) |
| Winch force | Separate force path in `FB1VehicleSimulation` (the S1-09 test force stays), applied at the winch point, counted as push in the drag caps. | ADR-007 amendment (2026-10-26) |
| Winch pull input | Native analog input on the jeep, like throttle. Not an ability. | ADR-007 amendment (2026-10-26) |
| Abilities | `UB1AbilitySet` data assets granted on the PlayerState ASC; input actions mapped to ability input tags (Lyra style). | ADR-014 (proposed) |

**Slope hold, the model.** Per mud wheel, a hold force opposes the net downhill force along the ground (gravity along the ground minus the tire's uphill push), up to `SlopeHoldVsSinkDepth(SinkDepth cm) × wheel load`. It's a static-friction-like term: it only cancels a net downhill force, so it never helps the jeep up the slope. The curve is over **absolute** sink depth (cm) and lives on the vehicle (`UB1VehicleMovementComponent`, `Mud|Config`, next to `BellyClearance`), because "how deep the jeep sits" is what holds it, whatever the preset. On flat ground gravity along the ground is zero, so the term is zero and the signed-off flat mud tuning doesn't change. Starting curve: ~0 at 0–15 cm (medium's resting depth of ~16 cm slides), rising to full hold around 30 cm (Deep's resting depth of ~31 cm holds; a fully dug-in medium wheel at 35 cm holds too).

## Phase order

| New | Was | Phase |
|---|---|---|
| 0, 1, G | 0, 1, G | unchanged (Sprint 1 closed, Sprint 2 active, not edited) |
| **2** | 4 | **On foot and winch** (mud + winch), Sprints 3–4 |
| **3** | 3 | **River** |
| **4** | 2 | **Damage and vegetation** (+ machete) |
| 5 | 5 | Scenario level (+ muddy uphill section on the forest path) |

Phases are renumbered so the number matches the order. ROADMAP.md gets a "was Phase N" note on each moved phase so HISTORY.md and the archived plan still read correctly.

## Scope

### Sprint 3 — Mud + winch core (Phase 2, part 1)

Ends with the **mud + winch checkpoint**: the user gets stuck in deep mud and on the muddy slope, attaches the winch with a cheat, pulls from the car, and retunes before any on-foot work starts.

1. **Slope spike + slope rules** (ADR-001 amendment). Rotated `BP_B1MudPatch` on a test slope. Implement the net-force standstill rule and the slope hold term, plus `SlopeHoldVsSinkDepth` live-editable in the Mud window. Check the hard-ground cap trace on a rotated patch (trace length `MaxSinkDepth / cos θ`). Pick the slope angle: the firm slope climbs at 50 %, medium mud fails at every throttle. If no angle does both, add a slope grip factor (ADR-001 again) instead of moving on. The mud suite (16) must stay green: the flat rules must not change.
2. **`Lvl_Gym_Winch`** (ADR-009). Lanes:
   - **Deep strip:** Deep (XL) patch, anchor tree on firm ground ~10 m past its far ramp;
   - **Slopes:** three lanes at the spike's angle and the same length, each starting from a flat run-up: firm (control), medium mud, deep mud; anchor tree at the top of each mud lane;
   - **Log lane:** a log across a firm lane;
   - spawns per lane; slope angle, length and anchor distances recorded in ARCHITECTURE.md.
3. **`UB1WinchComponent`** on the jeep front (ADR-007): attached anchor, rest length, `MaxCableLength`, `MaxPullForce`, `ReelInSpeed`, stiffness, damping. Game thread sends anchor location, rest length and pull through `FB1VehicleSimInput`. The sim applies a capped one-sided spring-damper with `AddForceAtPosition` at the winch point, keeps the body awake while attached, and feeds the pull into the drag caps. Rest length shrinks at `ReelInSpeed × pull` while taut, so tension ramps to the cap instead of stepping. `Vehicle.State.Winching` tag while pulling.
4. **`UB1WinchAnchorComponent`** (scene component) on placeholder tree actors (`BP_B1WinchTree`). Phase 4 adds obstacle data to the same tree.
5. **Winch input:** an analog `IA_B1WinchPull` (+ `IA_B1WinchRelease`) in the vehicle mapping context, read natively by the jeep.
6. **Rig + cheats.** `AB1GymTestRig`: optional `WinchAnchor` actor and a `WinchPull` value per input step. New metrics on the `B1Test` line: `maxH` (peak height above the rig), `tensionMax`, `tensionOsc` (tension amplitude in the last step), `anchorDistMax`. Cheats `B1WinchAttachNearest`, `B1WinchDetach`.
7. **Winch debug:** NetImGui "Winch" window (anchor, length vs max, tension N vs cap, pull, live-edit of the winch params with typed fields and save-to-asset per `imgui.md`), `B1Winch` telemetry line to `LogB1Physics`, `ShowDebug B1Winch`. Cable drawn with `UCableComponent` (enable the `CableComponent` plugin, add the module dep).
8. **Winch rig suite** (Validation below), headless, plus both existing suites.
9. **Checkpoint playtest** in `Lvl_Gym_Winch`.

### Sprint 4 — On foot (Phase 2, part 2)

1. **Ability plumbing (ADR-014):** `UB1AbilitySet` data asset (abilities + effects to grant), granted on the PlayerState ASC; `UB1InputConfig` mapping input actions to ability input tags; `AB1PlayerController` forwards pressed/released to the ASC.
2. **`AB1Character`** (first-person, `ACharacter`) + on-foot mapping context. `AB1PlayerController` swaps vehicle ↔ on-foot mapping contexts on possession.
3. **`GA_B1EnterExitVehicle`:** character hidden and attached to a seat socket while driving, engine off on exit (no fuel use), restart on enter, possession swap, ASC avatar swap and `State.Avatar.*` tags (ADR-004). Cheats `B1ExitVehicle`, `B1EnterVehicle`.
4. **`GA_B1CarryWinchHook`** (on foot): take the hook at the jeep's front, walk with it (the cable stops the character at `MaxCableLength`), attach to an anchor in range, or put it back. Cable visual follows the hand.
5. **Log pull:** a simulating log with an anchor component. The opposite cable force is applied in the log's async physics tick (fixed step), reading the cable state the winch publishes.
6. **Phase playtest** in `Lvl_Gym_Winch`, full loop on foot.

### Phase 2 stretch
Cable sag visuals, hook-in-hand mesh, enter/exit and winch animations, winch spool-out (free-spool) from the car, character slowed in mud.

### Moved scope
- **Machete** (`GA_B1Machete`) moves to Phase 4 (Damage and vegetation); it reuses the Sprint 4 ability plumbing.
- **"Winched, the jeep crosses the deep ford"** moves to Phase 3 (River).
- **Physics log** moves from old Phase 2 into Phase 2 (winch) as a winch target.
- **Phase 5:** the forest path gets a muddy uphill section (VISION.md, Muddy slope).

### Out of scope
- Any change to Sprint 2 or Phases 0, 1, G.
- River, damage, vegetation, machete in Phase 2.
- Cable breaking, winch damage, winch fuel/battery cost.
- Anchoring to arbitrary geometry (only actors with `UB1WinchAnchorComponent`).
- Physics rope or `UPhysicsConstraintComponent` cable (ADR-007).
- Changing the flat-ground mud tuning signed off in S1-24.

## Approach

The winch follows ADR-003 and ADR-007: a capped one-sided spring-damper inside `FB1VehicleSimulation` on the physics thread, with game-thread inputs through the lock-guarded `FB1VehicleSimInput` path that mud already uses. The S1-09 test spring proved this force shape stable at 20 and 120 fps; the winch is a sibling of it, not a replacement, so the stepping rigs keep working. It acts at the winch point, so a winched jeep pitches nose-up on the slope like a real one. Rolling resistance and belly drag move from "oppose the tire's push" to "oppose the net push along the ground" (tire + gravity + winch), which fixes the slope sign problem and lets the winch drag a belly-stuck jeep out. The slope hold term gives the user's rule directly: hold grows with absolute sink depth, only cancels a net downhill force, and is zero on flat ground. The on-foot side is Lyra style: ability sets on the PlayerState ASC, input tags for abilities, native input for continuous controls (throttle, steering, winch pull), and `AB1PlayerController` already accepts any pawn.

## Validation

All thresholds are starting values; the user's feel wins at the playtest. θ is the angle the spike picks.

| Check | Criterion | By |
|---|---|---|
| Flat mud unchanged | `Lvl_Gym_Mud` (16) and `Lvl_Offroad` (8) suites pass with the new rules, numbers within 5 % of Sprint 2 | Headless |
| Firm control slope | 50 % from the run-up reaches the top (`endH` ≥ top − 0.5 m) | Rig |
| Medium slope, unwinched | 15 s at 30 %, 50 % and 100 % from the run-up: none reaches the top (`maxH` ≤ top − 1 m) | Rigs |
| Medium slope, idle slide | Placed at mid-slope, zero throttle, 10 s: slides down ≥ 1 m, `vmax` ≤ 1.5 m/s | Rig |
| Deep slope, slide then hold | Placed at mid-slope, zero throttle, 10 s: slides ≥ 0.1 m, then moves ≤ 5 cm in the last 5 s | Rig |
| Dug in, can't climb | Medium slope after 3 s full throttle, then 10 s at 50 %: gains < 0.5 m height | Rig |
| Medium slope, winched | Anchor at the top, full pull + 50 % throttle: reaches the top in ≤ 30 s | Rig |
| Deep slope, winched | Same, deep lane: reaches the top in ≤ 45 s | Rig |
| Deep strip, winched | From the stuck state of a deep full-throttle dig-in, full pull + 50 %: on firm ground past the far ramp (`endH` at firm height) in ≤ 20 s | Rig |
| Winch stability | Full pull against a stuck jeep at 20 and 120 fps: `tensionMax` ≤ `MaxPullForce` + 5 %, `tensionOsc` non-growing, `peakOsc` ≤ 0.5 | Rigs |
| Cable limit | `anchorDistMax` ≤ `MaxCableLength` + 0.5 m while attached | Rig |
| Checkpoint | User: stuck in deep mud and on the medium slope, attach by cheat, winch out and up; feels right | User, end of Sprint 3 |
| Enter/exit | Headless `-game` script, 10 × `B1ExitVehicle` / `B1EnterVehicle`: no errors or ensures, fuel unchanged while outside, avatar tags correct after each | Headless + log |
| Log | A winched log moves ≥ 3 m toward the jeep | Rig |
| Phase sign-off | User: full loop on foot (exit, carry hook to the tree, enter, winch out) in deep mud and up the slope | User playtest, end of Sprint 4 |

## Risks

1. **The slope rules may not produce the decided behaviour** (slow slide when barely sunk, hold when deep, never climb unwinched) at an angle the firm slope climbs. Chaos tires roll freely downhill at zero throttle, so the slide speed comes from engine drag, rolling resistance and the hold term together. — Sprint 3 starts with the spike, the hold curve is live-editable, the flat suites guard against regressions, and the fallback is a per-material slope grip factor (ADR-001).
2. **Winch vs the drag caps.** A belly-stuck jeep under full pull is the core mud + winch moment. If the net-push change is wrong, the jeep either won't move at the cap or shoots out. — Deep-strip winched rig plus the stability rigs at 20/120 fps; tension and drag both in the telemetry line.
3. **Ability and input plumbing** is the first real GAS ability use and the first mapping-context swap; possession with a Chaos vehicle (sleep, engine state, camera, template respawn/flip logic) breaks in subtle ways. — Own task (ADR-014) before the character, enter/exit cheats for headless checks, both suites re-run.
4. **Winch at the cap vs Chaos low-speed behaviour** (high slip, near-zero speed, sleeping body). — No sleep while attached (S1-09 rule), stability rigs.
5. **Log force off the vehicle sim.** Async physics tick on an actor is less travelled than the vehicle sim. — Fallback: game-thread `AddForce` on the log only (light body, low stakes).
6. **Rotated mud patch hard-ground cap** (vertical trace, `depth / cos θ`). Small. — Spike check, lengthen the trace.

## Dependencies

- Sprint 2 closed (GAS bootstrap, `AB1PlayerController` any-pawn, fuel for engine-off on exit).
- ADR-001 and ADR-007 amendments (2026-10-26), ADR-014 (proposed), ADR-003, ADR-004, ADR-009.
- `CableComponent` plugin: not enabled in `MudDriver.uproject` yet; enable it and add the module dep in Sprint 3.
- Mud presets and `BP_B1MudPatch` from Sprint 1.

---

## Critique (round 1) — 2026-10-26

### Architectural alignment — 4/5
The winch follows ADR-003 and ADR-007 as written, and the game → physics path (`FB1VehicleSimInput` under a lock) is already proven by mud. ADR-009 still holds: gyms first, scenario last; the reorder doesn't touch it. Three things are off:
- "Replaces the S1-09 test spring" breaks existing rigs. `B1Step_Spring20/120` (the ADR-003 stepping proof) depend on `EB1TestForce::Spring`. The winch should be a separate force path next to the test force, not a replacement.
- The plan doesn't say where the winch force acts. It must be `AddForceAtPosition` at the winch point (front bumper), not at the centre of mass, or the nose won't lift on the slope and the jeep won't pitch the way a winched car does. That's an ADR-007 detail worth writing down when it moves from proposed to accepted.
- There is no ability plumbing in the code yet: nothing calls `GiveAbility`, and abilities have no input binding (`grep GiveAbility|AbilitySet Source/` is empty). The plan names three abilities (`GA_B1EnterExitVehicle`, `GA_B1CarryWinchHook`, `GA_B1WinchControl`) as if granting and input activation exist. Lyra does this with an ability set data asset granted on the PlayerState plus input tags mapped to abilities; that's a task on its own, and it's the first real use of GAS abilities in the project.

### Scope clarity — 3/5
The reorder table and the "moved scope" list are clear, and moving the machete and the ford criterion out is the right call. Two gaps:
- **Input.** The character needs its own mapping context, and the controller has to swap vehicle ↔ on-foot contexts on possession. The analog winch pull also needs an input action in the vehicle context. None of that is listed, and it's where possession bugs hide (risk 5).
- **No mid-phase proof point.** The success criterion is "mud + winch first", but the only user check is the phase-end playtest after the character, enter/exit, hook carry and log. With the cheat attach and an in-car pull key, the user can feel winching out of deep mud and up the slope after the winch core alone. That should be an explicit checkpoint, with the user able to retune before any character work.
- Item 1 of the gym leaves the slope angle and the anchor distances open. Fine for a plan, but the spike must write them back before rigs are built on top.

### Validation specificity — 3/5
Most criteria are numeric and rig-checkable. Weak spots:
- "ends lower than its highest point by ≥ 1 m" needs a metric the rig doesn't have (`maxH`, peak height above the rig). Same for tension amplitude in the stability check, and for the cable-limit check (max distance to the anchor). List the new rig metrics.
- The firm control slope needs a throttle: "full throttle reaches the top" is too easy and doesn't prove the contrast. Use 50 %, the same throttle the mud slope fails at.
- Enter/exit is "rig or headless script". The agent can't press keys, so it needs cheats (`B1ExitVehicle`, `B1EnterVehicle`) that activate the ability, run from a headless `-game -ExecCmds` script. Name them.
- Idle on the muddy slope isn't covered: what does a jeep stopped on medium mud on the slope do with zero throttle? It should slide back or hold; either is a design answer, but it must be decided and tested, see the next point.

### Risk realism — 3/5
Risk 1 is the right top risk, but the plan describes it vaguely. Reading `B1VehicleSimulation.cpp:229-235` makes it concrete: at standstill, rolling resistance opposes `GetForceFromFriction().X`, treating the tire force as "the push". On flat ground that's the engine. On a slope, at rest, the tire force is mostly the tire holding the jeep against gravity, so rolling resistance gets applied **downhill**, against the holding force. While sliding back, the opposite happens: motion is downhill, `TirePush` is 0, and the cap becomes `StopForce = m·v/dt·0.4`, a strong brake against the slide up to `Coefficient × WeightShare`. Belly drag (`:293-296`) has the same pattern. So the slope behaviour can be both "creeps downhill at idle" and "slide is damped hard", and neither was designed. That needs a gravity-aware rule (subtract the gravity component along the wheel forward from the tire force before treating it as push), which is an ADR-001 amendment, not just "tune the angle".

Risk 2 (rotated patches) is smaller than stated: `AB1MudPatch` already builds its components with the actor rotation (`B1MudPatch.cpp:88`). What's left is the vertical hard-ground cap trace: on a θ slope the distance to the hard bottom is `depth / cos θ`, ~6 % at 20°, and the trace length is `MaxSinkDepth`, so it can miss the bottom and leave the wheel uncapped. Minor, a spike check.

Missing risks: the ability/input plumbing above (the first GAS abilities, and the first Enhanced Input context swap), and **winch tension at the cap with a stuck, belly-dragged jeep**: belly drag and rolling resistance are capped at "push + stopping force" and the winch is a new external push. If the winch force isn't counted as push, a deep-mud jeep under full pull might not move at all, because the drag cap only lets the *tires'* push through. That's the core mud + winch interaction and it's not mentioned.

### Agent ergonomics — 4/5
Good: the rig gets `WinchAnchor` and per-step `WinchPull`, there's a telemetry line, and the winch core is rig-testable before the character exists. Add the rig metrics above (`maxH`, `tensionMax`, `tensionOsc`, `anchorDistMax`) to the same `B1Test` line so results chain with the existing suites, and the enter/exit cheats so possession can be checked headless.

### Implementability — 2/5
This is two sprints, not one. Rough count: slope spike + gym (1 d), winch component + sim force + anchor (1.5 d), rig support + cheats + debug window (1 d), winch rigs incl. 20/120 fps (0.5 d) — that's Sprint 3, and it already proves mud + winch. Then ability set + input plumbing (1 d), character + enter/exit + context swap (1.5 d), hook carry + cable limit + in-car control ability (1.5 d), log via async tick (0.5–1 d), playtest — Sprint 4. Split the phase along that line, with the user checkpoint at the end of Sprint 3.

### Recommended changes
1. **Keep the test force.** The winch is a new force path in `FB1VehicleSimulation`, next to `EB1TestForce`; the stepping rigs stay.
2. **Winch force at the winch point** (`AddForceAtPosition`), and count it as push in the rolling-resistance and belly-drag caps, so the stuck jeep can be pulled out. Write both into ADR-007 when it's accepted.
3. **Make the slope spike gravity-aware.** Fix the standstill rule to subtract the gravity component from the tire force (or otherwise make it slope-safe), test idle and slide-back on the muddy slope, and amend ADR-001. Record the chosen angle and the anchor distances in the gym task.
4. **Add an ability + input task:** a `UB1AbilitySet` data asset granted on the PlayerState, input tags → ability activation (Lyra style), on-foot mapping context and the context swap in `AB1PlayerController`, winch pull input action. Add an ADR for ability granting / input binding.
5. **Split Phase 2 into two sprints**, with a **"mud + winch via cheat" user checkpoint** at the end of Sprint 3 (deep mud and slope, attach by cheat, pull from the car).
6. **Tighten validation:** firm control slope at 50 %; new rig metrics `maxH`, `tensionMax`, `tensionOsc`, `anchorDistMax`; cheats `B1ExitVehicle` / `B1EnterVehicle` for headless possession checks; a decided idle-on-slope behaviour with a rig.
7. **Rewrite Risks:** slope standstill rule (concrete, as above) and winch vs drag caps at the top; downgrade rotated patches; add ability/input plumbing.
8. **Dependencies:** `CableComponent` isn't enabled in `MudDriver.uproject` yet; enable the plugin and add the module dep.

### Verdict
- [ ] Ready to convert to roadmap (run `/plan-roadmap`)
- [x] Needs revision (run `/plan-replan`)

---

## Revision 1 — 2026-10-26

### Changes applied from critique
- **#1 Test force kept.** The winch is a sibling force path in `FB1VehicleSimulation`; `B1Step_Spring/Push` rigs stay.
- **#2 Winch at the winch point, counted as push.** `AddForceAtPosition` at the winch point; drag caps use the net push (tire + gravity + winch). ADR-007 amended.
- **#3 Gravity-aware slope rules, with the user's decision (2026-10-26):** a stopped jeep on a muddy slope slides down slowly; hold depends on **absolute sink depth**, not the preset. Barely sunk = slides, deeply sunk = held but can't climb, Deep slides until it sinks in, then stops. Modelled as a `SlopeHoldVsSinkDepth` curve on the movement component that only cancels a net downhill force, plus the net-force standstill rule. Zero on flat ground, so the S1-24 sign-off holds. ADR-001 amended. The spike records the angle and anchor distances.
- **#4 Ability + input task.** Sprint 4 task 1: `UB1AbilitySet`, `UB1InputConfig`, controller forwarding, on-foot mapping context and swap. New ADR-014 (proposed).
- **#5 Two sprints** with a mud + winch checkpoint at the end of Sprint 3 (cheat attach, pull from the car).
- **#6 Validation tightened.** Firm control slope at 50 %; rig metrics `maxH`, `tensionMax`, `tensionOsc`, `anchorDistMax`; cheats `B1ExitVehicle` / `B1EnterVehicle`; idle-slide, slide-then-hold and dug-in rigs; deep slope lane added to the gym.
- **#7 Risks rewritten.** Slope rules and winch vs drag caps on top; ability/input plumbing added; rotated patch downgraded to last.
- **#8 `CableComponent`** plugin + module dep listed in Dependencies and in Sprint 3 task 7.
- **Beyond the critique:** winch pull is native analog input on the jeep, not `GA_B1WinchControl` (continuous control like throttle; Lyra keeps move/look native too). That drops one ability and lets the Sprint 3 checkpoint work without ability plumbing.

### Changes deferred (with reason)
- **ADR-007 to accepted:** stays proposed until the Sprint 3 stability rigs pass; the amendment records the decisions made now.
- **Exact slope angle, hold curve and anchor distances:** the Sprint 3 spike picks them; starting values are in Design decisions.
- **Breaking Phases 3–5 into tasks:** done in their own replans, as before.

### ADR handling
- **ADR-001 amended, not superseded.** Per-wheel dynamic sinking still holds; the slope hold and net-force standstill rule extend it to slopes and leave flat ground untouched.
- **ADR-007 amended, not superseded.** The tension-force decision holds; the amendment fixes where it acts, how it meets the drag caps, and that pull is native input.
- **ADR-014 new.** Ability granting and input binding is a separate decision with its own alternatives.
- **ADR-009 unchanged.** Gyms first, scenario last; only the order of the gyms changed.

### Verdict
- [x] Ready to convert to roadmap
