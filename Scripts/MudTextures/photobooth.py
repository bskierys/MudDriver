# Photo booth for the dirt and lens looks. Not a local script: paste it as the `script` of
# ProgrammaticToolset.execute_tool_script (Unreal MCP) with Lvl_Gym_Core loaded.
# Returns one base64 PNG per capture; stage_sheet.py decodes them (--lens for the lens keys).
# At most 2 captures per call (more overflowed / closed the MCP connection): edit STAGES / LENS_LEVELS.
# Dirt: keys "d050_front"...; leaves PreviewDirt at -1.
# Lens: keys "l050" (all 4 slots of MI_B1LensMud_Preview at 0.5) and "lnone" (volume disabled), from the
# chase pose; enables B1PhotoBooth_LensPreview for the capture, leaves it disabled and the MI's slots at 1.
import json, time

MODE = 'lens'  # 'dirt' or 'lens'
JEEP = '/Game/B1/Gym/Lvl_Gym_Core.Lvl_Gym_Core:PersistentLevel.BP_B1Jeep_Wrangler_C_UAID_7CB566F64C2FBB1203_1269337751'
STAGES = [0.5]  # dirt, full set: 0, 0.25, 0.5, 0.75, 1
# x, y, z, pitch, yaw. Jeep at (-2000, -13000, 2), yaw 115: the sun (yaw ~166) rakes both angles from the side.
# The two concept angles face opposite ways, so at any other yaw one of them is backlit.
POSES = {
    'front': [-1855, -12458, 150, -6, -105.0],  # front three-quarter left
    'rear': [-2145, -13542, 165, -7, 75.0],     # rear three-quarter right
}

LENS_LEVELS = [None, 0.0]  # None = volume disabled; full set: None, 0, 0.25, 0.5, 0.75, 1
LENS_VOLUME = '/Game/B1/Gym/Lvl_Gym_Core.Lvl_Gym_Core:PersistentLevel.PostProcessVolume_UAID_7CB566F64C2FEE1203_1507231727'
LENS_MI = '/Game/B1/FX/MI_B1LensMud_Preview.MI_B1LensMud_Preview'
# The back camera: 680 cm behind the jeep, 255 cm up, pitch -7.
CHASE = [-1713, -13616, 257, -7, 115.0]
# (offset U, offset V, variant) per slot, as saved on the MI
LENS_SLOTS = [(0.13, 0.71, 0), (0.58, 0.29, 3), (0.87, 0.52, 5), (0.34, 0.06, 6)]


def call(tool, args):
    return execute_tool(tool, json.dumps(args))


def check_level():
    level = call('editor_toolset.toolsets.scene.SceneTools.get_current_level', {})['returnValue']
    if level != '/Game/B1/Gym/Lvl_Gym_Core':
        raise RuntimeError('load Lvl_Gym_Core first, the editor has ' + level)


def set_props(path, values):
    call('editor_toolset.toolsets.object.ObjectTools.set_properties', {'instance': {'refPath': path}, 'values': json.dumps(values)})


def set_preview(v):
    set_props(JEEP + '.MudDirt', {'PreviewDirt': v})


def set_lens(strength):
    for i, (u, v, var) in enumerate(LENS_SLOTS):
        call('editor_toolset.toolsets.material_instance.MaterialInstanceTools.set_vector_parameter',
             {'instance': {'refPath': LENS_MI}, 'name': 'LensSlot%d' % i, 'value': {'r': strength, 'g': u, 'b': v, 'a': float(var)}})


def cap(p):
    x = {'location': {'x': p[0], 'y': p[1], 'z': p[2]}, 'rotation': {'pitch': p[3], 'yaw': p[4], 'roll': 0},
         'scale': {'x': 1, 'y': 1, 'z': 1}}
    a = {'gridSpacing': 0, 'gridExtent': 0, 'gridHeight': 0, 'maxLabelDistance': 0, 'classFilter': None, 'maxLabels': 0}
    return call('EditorToolset.EditorAppToolset.CaptureViewport', {'captureTransform': x, 'annotations': a})['returnValue']['image']['data']


def run_dirt():
    out = {}
    for s in STAGES:
        set_preview(s)
        time.sleep(0.5)
        for name, pose in POSES.items():
            out['d%03d_%s' % (int(s * 100), name)] = cap(pose)
    set_preview(-1.0)
    return out


def run_lens():
    out = {}
    try:
        for s in LENS_LEVELS:
            set_props(LENS_VOLUME, {'bEnabled': s is not None})
            if s is not None:
                set_lens(s)
            time.sleep(0.5)
            out['lnone' if s is None else 'l%03d' % int(round(s * 100))] = cap(CHASE)
    finally:
        set_props(LENS_VOLUME, {'bEnabled': False})
        set_lens(1.0)
        call('editor_toolset.toolsets.asset.AssetTools.save_assets', {'asset_paths': ['/Game/B1/FX/MI_B1LensMud_Preview']})
    return out


def run():
    check_level()
    return run_lens() if MODE == 'lens' else run_dirt()
