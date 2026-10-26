---
title: Project Overview
type: overview
engine: UNREAL · Unreal Engine 5.8 · (C++)
updated: 2026-10-26
stale: false
---

# MudDriver — Project Overview

> Engine: UNREAL · Unreal Engine 5.8 · (C++)  |  Source: `E:\GameDev\Projects\MudDriver\Source`
> Generated: 2026-10-26
> Regenerate: `gdep init <project_path> --force`

## Codebase Snapshot

| Metric | Value |
|--------|-------|
| Files | **38** |
| Classes | **37** |
| Circular deps | **0** |
| Dead code candidates | **0** |

> **Dead code** = classes with in-degree 0 (no other class references them in code).
> Unity: prefab/scene asset references are also checked — a class used only from a prefab is NOT dead code.
> UE5: Blueprint, ABP, Montage, and GAS .uasset references are also checked — a class used only from a .uasset is NOT dead code.
> Generic C++: only source-code references are checked.

### High-coupling classes (likely God Objects — modify with care)

- `AMudDriverPawn` — 6 dependents
- `UB1PhysicalMaterial_Mud` — 5 dependents
- `UMudDriverUI` — 4 dependents
- `ATimeTrialTrackGate` — 3 dependents
- `FB1TestForceSettings` — 2 dependents

## GAS Summary

- Abilities (C++): 0
- Effects (C++): 0
- AttributeSets: 1
- GameplayTags (in assets): 0 high-confidence / 18 low
- GAS .uassets — IS-A: GA 0 / GE 3 / AS 0 / ABP 1  |  Referencers: 7
- Analysis confidence: **HIGH** (cpp_source_regex + binary_pattern_match)
- Coverage: 129/129 assets (100.0%)

---
See [[index]] for the full wiki page catalog.
