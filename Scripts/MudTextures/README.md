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
