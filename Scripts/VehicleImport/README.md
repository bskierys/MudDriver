# vehicle import

Turns the Sketchfab Wrangler FBX into the assets in `/Game/Vehicles/Wrangler`. Needs Python 3 with `numpy` and `pillow`.

```
python convert_wrangler.py <Jeep_wrangler_1997.fbx> <out_dir> [cm_per_unit=19.5] [wheel_scale=1.2] [lift_cm=6] [track_out_cm=3]
python pack_textures.py <sketchfab textures dir> <out_dir>
```

- `convert_wrangler.py` writes `SM_Wrangler_Body.fbx`, `SM_Wrangler_Tire.fbx` (front-left wheel, centred) and `SKM_Wrangler.fbx` (axles + hubs on `OffroadCar` / `PhysWheel_*` bones), ASCII FBX in UE axes. Wheels are scaled about their centres, moved down by the lift and out by the track offset, then everything is re-grounded. After a change, update the wheel classes' radius/width and the physics asset shapes to match `wrangler_info.json`.
- The import tool won't overwrite: delete `SM_Wrangler_Body`, `SM_Wrangler_Tire` and `SKM_Wrangler` first, then reassign materials, sockets, the physics asset and the `BP_B1Jeep_Wrangler` mesh references.
- Import the SKM against `SKM_Offroad_Skeleton`, then add `VisWheel_*` sockets on `PhysWheel_*`.
- `pack_textures.py` writes 2K `T_Wrangler_*_D/_N/_MRA` PNGs (MRA = metallic, roughness, AO; normal green flipped to DirectX).
