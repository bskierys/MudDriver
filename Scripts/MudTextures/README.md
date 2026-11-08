# mud textures

Generates the dirt masks for `MF_B1MudDirt` and the spray clump mesh. Needs Python 3 with `numpy` and `pillow`.

```
python gen_mud_textures.py <out_dir> [seed=7]
```

Output (fixed seed, same files every run):

| File | Asset | Notes |
|---|---|---|
| `T_B1MudBlotch.png` | `/Game/B1/Mud/Dirt/T_B1MudBlotch` | 1K tileable, large soft blotches |
| `T_B1MudSplatter.png` | `/Game/B1/Mud/Dirt/T_B1MudSplatter` | 1K tileable, small splats, some with drips running down (+V); 0 outside the splats, ~12 % covered |
| `T_B1MudNoise.png` | `/Game/B1/Mud/Dirt/T_B1MudNoise` | 512 tileable breakup |
| `SM_B1MudClump.obj` | `/Game/B1/FX/SM_B1MudClump` | lumpy blob, 48 tris, ~1 cm radius, centred |
| `contact_sheet.png` | — | each mask next to a 2×2 repeat, to check seams |

The masks are reveal-order fields: histogram-equalized to 0..1, so a threshold `t` in the material covers a fraction `t` of the area. Brighter = gets dirty earlier. In the splatter mask, bigger splats tend to arrive later.

Parameters are the constants at the top of the script (sizes, blotch spectrum, splat count/radius/drip chance, clump segments and lumpiness).

## Re-import

`TextureTools.import_file` / `StaticMeshTools.import_file` won't overwrite. Delete the asset first, re-import, then:

- textures: `SRGB` off, `CompressionSettings` `TC_Grayscale`;
- mesh: Nanite stays off;
- reassign the references (`MF_B1MudDirt` texture params, the spray's mesh renderer).

## Lens fields

```
python gen_lens_textures.py <out_dir> [seed=22] [Concept/mud-on-the-screen.jpg]
```

Drop field for `M_B1LensMud` (ADR-022), drops only (no specks: the concept's dots were an image artifact). One 2048×1024 tile = one splash slot; the material stacks 4 slots. Tile height maps to screen height (1 px ≈ 1 px at 1080p), tiles wrap in both axes.

| File | Asset | Channels |
|---|---|---|
| `T_B1LensDrops.png` | `/Game/B1/FX/Lens/T_B1LensDrops` | RG drop normal xy (0.5-biased), B 1 − normalized radius (0 outside), A reveal id (background 1) |
| `lens_contact_sheet.png` | — | concept next to a rough numpy stand-in of the material at 0.25 / 0.5 / 0.75 / 1 (flat colours, so refraction barely shows) |

Reveal ids are spread evenly over the drops, so `id < strength` shows that fraction; bigger ones tend to come later. Outlines wobble and some drops have a smaller blob merged in. Slot variants (0–7): bit 0 flips U, bit 1 flips V, bit 2 inverts the reveal order (only inside drops). Parameters are the constants at the top (count, radii, aspect, wobble, satellites). Import: `SRGB` off, `CompressionSettings` `TC_VectorDisplacementmap`, `MipGenSettings` `TMGS_NoMipmaps`, `LODGroup` `TEXTUREGROUP_UI`.

Stills: `photobooth.py` with `MODE = 'lens'` (2 levels per call), then `stage_sheet.py --lens <out_dir> <tool-result files...>`.

## Spray sheet

Shoot `B1Spray_Medium` in `Lvl_Gym_Mud` with a `ProgrammaticToolset.execute_tool_script` that sets the camera to (3300, 2450, 110), pitch −3, yaw 158, sets the rig's `bAutoRun` on, runs `StartPIE` (Simulate), calls `CaptureEditorImage` into keys `c10` / `c20` / `c30` (tenths of a second into the rig script: warmup 2.5 s = t 1 s), then `StopPIE` and `bAutoRun` off. Two captures per call at most. Then:

```
python spray_sheet.py <out_dir> <tool-result files...>
```

Writes the captures and `spray_sheet.png` (zoomed 3D view next to frames of `Concept/mud-car-drive.mp4`). Also needs `opencv-python-headless`.
