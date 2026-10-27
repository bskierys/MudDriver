# PLAN

Single active plan, zeroed by `/plan-roadmap`. Start a new one with `/plan-new`.

_No active plan._

Closed plans are archived in [plans/](plans/). Last one: [Mud + winch first](plans/2026-10-26-mud-winch-first.md) → [ROADMAP.md](ROADMAP.md) Phase 2 (On foot and winch, was Phase 4), Sprints 3–4 in [SPRINTS.md](SPRINTS.md).

---

## Critique (round 1) — 2026-10-27 — Sprint 4 (S4-03..S4-10)

Subject: the refined Sprint 4 in [SPRINTS.md](SPRINTS.md), not a PLAN.md plan (none is active). S4-01 and S4-02 are built (uncommitted); this reviews the open tasks against ARCHITECTURE.md, ADR-004, ADR-007 and ADR-014 (accepted with S4-02).

### Architectural alignment — 3/5
The task order follows ADR-014 and ADR-004: abilities are granted once on the PlayerState, the avatar swaps on possession, and the mapping contexts swap in the controller. Pull stays native, per ADR-007. Two gaps:
- **No interaction design.** S4-06 (enter), S4-07 (take / attach / return the hook) and S4-08 (prompt) all go through one "interact" key, but no task says which ability wins. At the jeep's front, "enter" and "take hook" are both valid, and both abilities are gated only by `State.Avatar.OnFoot`. Lyra solves this with an interaction scan: `IInteractableTarget` returns options, one `GA_Interact` picks the best and triggers the option's ability, and the prompt reads the same scan. Without that, S4-06 and S4-07 each grow their own range checks, and S4-08 duplicates them a third time. This deserves an ADR (ADR-017).
- **`State.Avatar.*` ownership left open** ("pick one" in S4-06). ADR-004 says the tags gate everything, so the owner should be fixed now. Keep it in `AB1PlayerController::OnPossess`, keyed on pawn type: cheats, rigs and respawn also possess pawns without going through the ability.

The carried-hook state also has no home in the winch design. `UB1WinchComponent` today is either attached (anchor in `FB1WinchSimInput`, spring force) or detached. If the hand becomes an anchor with rest = `MaxCableLength`, a character walking past 80 m would pull the jeep with up to 60 kN. "Carried" has to be a third state: visual only, no sim input, with the leash on the character side.

### Scope clarity — 3/5
Each task's content is clear, but the sprint has no out-of-scope line, and several edge states are undefined. Each one becomes a mid-task decision:
- Entering the jeep while carrying the hook (block it, or put the hook back automatically).
- The jeep sliding away while you carry the hook: the user decided a parked jeep slides on muddy slopes, so this will happen. Does the leash drag the character, stop them, or drop the hook?
- Taking the hook while it's attached to a tree. Is detaching only from the car, or also at the anchor?
- What happens to `Vehicle.State.InMud` / `Winching` on the ASC while on foot (ADR-004: "cleared by the jeep, not by possession").

The roadmap stretch list (animations, hook mesh, free-spool, character slowed in mud) should be copied into the sprint as explicitly out of scope. On-foot gamepad mappings aren't mentioned. The user plays with a pad (D-pad winch layout), so `IMC_B1OnFoot` needs pad bindings from the start.

### Validation specificity — 3/5
S4-05, S4-06 and S4-09 have rig checks with numbers. The rest are weak:
- **S4-03** "walks in PIE (capture)": the agent can't drive input, so a capture only shows a standing character. Neither "walks" nor "stands on the hard bottom" is checked by the agent.
- **S4-07** uses `TeleportPawnTo` instead of walking, which bypasses the leash, the feature most likely to misbehave. The 80 m leash is never tested by the agent.
- **S4-08** Done is "builds" only. Fine for the prompt, but the hook-state debug output could be checked in the S4-07 rig log.
- **S4-05** `B1Park_FirmSlope` asks for `lastDisp` ≤ 5 cm over 10 s. That's untested: since ADR-016 removed Chaos's anti-slide, nothing holds a jeep on firm ground except tire friction and sleep (`Push20/120` drift went 0.7 → 12.7 m on flat ground once the slope stopped holding it). Locked rear wheels in the Chaos tire model may creep. Measure first, then set the threshold.

A small character step in the rig (move input along a direction for the step's duration on a possessed `AB1Character`) fixes S4-03 and S4-07 together. With it: walk over a deep patch and check the capsule Z against the hard bottom; walk 100 m away with the hook and check the distance stays ≤ 80.5 m.

### Risk realism — 2/5
The sprint names only research items (`SetParked`, async physics tick) and one creep note. The real top three:
1. **Possession inside an ability.** `GA_B1EnterExitVehicle` calls `Possess`, which calls `InitAbilityActorInfo` with a new avatar while the ability is active on the old one. Instanced abilities get `OnAvatarSet`, and ending the ability after the swap can hit stale actor info. Mitigation: keep the ability thin and instant. It validates and commits, asks the controller or game mode to do the swap, and ends in the same frame. The swap code also serves the cheats and rigs.
2. **Seat + rigs + respawn.** "The character is spawned once by the game mode and seated" is meant for one jeep. But `bPossessWithPlayer` rigs spawn their own jeep and possess it, and `OnPawnDestroyed` respawns a jeep. The character must follow possession: possessing a jeep re-seats the character in it. Otherwise the 10× exit/enter rig exits from the wrong jeep. An attached character on a simulating body also needs movement off (`MOVE_None`) as well as collision off.
3. **The carry state in the winch** (see Architectural alignment). It's the heart of the phase loop, and a hand anchor fed to the sim would be a 60 kN bug that a teleport-based rig can't catch.

The log (S4-09) is lower risk than it looks. Since the winch force is already computed in `FB1VehicleSimulation` on the physics thread, the opposite force could be applied to the log's body handle in the same step. That makes it exactly equal and opposite, with no cross-thread read. Treat it as the first research option before `AsyncPhysicsTickActor`, and amend ADR-007 if it wins.

### Agent ergonomics — 3/5
The ability-input rig step (S4-02) was the right call and keeps on-foot work agent-verifiable. Missing:
- the move step above;
- metrics the rigs can threshold on: possessed pawn class, avatar tag, hook state, carry distance, character Z;
- a debug view for the first real abilities. CLAUDE.md asks for proper debug tools per feature. A small NetImGui "Abilities" panel (granted specs with input tags, active abilities, ASC loose tags, current avatar) is cheap and pays off through Phase 4 (machete). `showdebug abilitysystem` exists, but it's a user tool.

The cheats `B1ExitVehicle` / `B1EnterVehicle` should call the same controller swap as the ability, so cheats, rigs and abilities can't drift apart.

### Implementability — 4/5
About 20.5 h is left, which is sprint-sized for one developer. Two estimates are tight: S4-06 at 3 h (door placement with fallbacks, avatar swap, re-seat, cheats, 10× rig) and S4-07 at 3.5 h (new winch state, leash, cable to hand, toggle change, rig steps). The interaction scan would add about 2 h, but it takes work out of S4-06, S4-07 and S4-08. S4-09 (log) is independent and can run in parallel or slip to Phase 3 without hurting the on-foot loop. If the sprint runs long, that's the split.

### Recommended changes
1. **Add an interaction task before S4-06** (~2.5 h, ADR-017): `IB1Interactable` returns options (text + ability or event tag) for a character in range; one `GA_B1Interact` on `InputTag.Interact` picks the best option and triggers it. The jeep offers Enter (door) and Take / Return hook (front); anchors offer Attach / Detach. S4-08's prompt reads the same scan, so S4-08 shrinks to the UI and debug parts.
2. **Fix the avatar tag owner:** `State.Avatar.*` stays in `AB1PlayerController::OnPossess`, keyed on pawn type. Remove the "pick one" from S4-06.
3. **Keep enter/exit thin:** the controller does the swap (`ExitVehicle` / `EnterVehicle`); the ability and cheats call it, and the ability ends in the same frame.
4. **Character follows possession:** possessing a jeep seats the character in it, so rigs and respawn work. Seated means hidden, collision off and `MOVE_None`. Add this to S4-05 with a rig that possesses a rig-spawned jeep and exits it.
5. **Define the winch hook states** in S4-07: stowed / carried / attached. Carried sends nothing to the sim; the leash is enforced on the character. Settle these edge cases in the task text:
   - a sliding jeep at full length drags the character (recommended);
   - entering while carrying returns the hook automatically;
   - detaching works from the car (toggle) and at the anchor (interact).
6. **Add a character move step to the rig** (S4-03) plus the metrics pawn class, avatar tag, hook state, carry distance and character Z. Then:
   - S4-03 checks the hard bottom with a walk over deep mud;
   - S4-07 adds `B1OnFoot_Leash` (walk 100 m away with the hook, distance ≤ 80.5 m);
   - teleport stays only for long trips.
7. **S4-05 firm park:** measure the creep first and set the threshold from it. If the creep is big, hold the parked jeep on firm ground through Chaos sleep (no keep-awake off mud) rather than adding a hold force.
8. **S4-09 research order:** try applying the opposite force from `FB1VehicleSimulation` to the log's body handle in the same physics step first; `AsyncPhysicsTickActor` second; game-thread `AddForce` last. Mark S4-09 as the task to slip if the sprint runs long.
9. **Add "Out of scope (Sprint 4)"** from the roadmap stretch list, and require gamepad bindings in `IMC_B1OnFoot`.
10. **Add a NetImGui "Abilities" panel** (1 h) to S4-08: granted specs and input tags, active abilities, loose tags, avatar.

### Verdict
- [ ] Ready to convert to roadmap (run `/plan-roadmap`)
- [x] Needs revision (run `/plan-replan`)

Applied 2026-10-27 with `/sprint` directly in SPRINTS.md: all 10 changes. New task S4-11 (interaction scan, ADR-017); the user picked a Lyra-style scan, the leash drags the character, getting in stows the hook, and the Abilities panel goes in S4-08.
