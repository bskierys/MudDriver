---
title: Project Overview
type: overview
engine: UNREAL · Unreal Engine 5.8 · (C++)
updated: 2026-10-28
stale: false
---

# MudDriver — Project Overview

> Engine: UNREAL · Unreal Engine 5.8 · (C++)  |  Source: `E:\GameDev\Projects\MudDriver\Source`
> Generated: 2026-10-28
> Regenerate: `gdep init <project_path> --force`

## Codebase Snapshot

| Metric | Value |
|--------|-------|
| Files | **64** |
| Classes | **68** |
| Circular deps | **0** |
| Dead code candidates | **0** |

> **Dead code** = classes with in-degree 0 (no other class references them in code).
> Unity: prefab/scene asset references are also checked — a class used only from a prefab is NOT dead code.
> UE5: Blueprint, ABP, Montage, and GAS .uasset references are also checked — a class used only from a .uasset is NOT dead code.
> Generic C++: only source-code references are checked.

### High-coupling classes (likely God Objects — modify with care)

- `AB1Character` — 5 dependents
- `UB1PhysicalMaterial_Mud` — 5 dependents
- `AMudDriverPawn` — 4 dependents
- `UB1GameplayAbility_WinchHook` — 4 dependents
- `UB1MudVehicleComponent` — 4 dependents

## GAS Summary

- Abilities (C++): 9
- Effects (C++): 0
- AttributeSets: 1
- GameplayTags (in assets): 0 high-confidence / 22 low
- GAS .uassets — IS-A: GA 8 / GE 3 / AS 0 / ABP 1  |  Referencers: 8
- Analysis confidence: **HIGH** (cpp_source_regex + binary_pattern_match)
- Coverage: 149/149 assets (100.0%)

---
See [[index]] for the full wiki page catalog.
