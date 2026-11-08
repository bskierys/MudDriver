# Photo booth for the dirt look. Not a local script: paste it as the `script` of
# ProgrammaticToolset.execute_tool_script (Unreal MCP) with Lvl_Gym_Core loaded.
# Returns one base64 PNG per stage and pose ("d050_front", ...); stage_sheet.py decodes them.
# Run once per stage (edit STAGES): more than ~2 captures per call overflows the MCP reply.
# Leaves PreviewDirt at -1.
import json, time

JEEP = '/Game/B1/Gym/Lvl_Gym_Core.Lvl_Gym_Core:PersistentLevel.BP_B1Jeep_Wrangler_C_UAID_7CB566F64C2FBB1203_1269337751'
STAGES = [0.5]  # full set: 0, 0.25, 0.5, 0.75, 1
# x, y, z, pitch, yaw. Jeep at (-2000, -13000, 2), yaw 115: the sun (yaw ~166) rakes both angles from the side.
# The two concept angles face opposite ways, so at any other yaw one of them is backlit.
POSES = {
    'front': [-1855, -12458, 150, -6, -105.0],  # front three-quarter left
    'rear': [-2145, -13542, 165, -7, 75.0],     # rear three-quarter right
}


def set_preview(v):
    level = execute_tool('editor_toolset.toolsets.scene.SceneTools.get_current_level', '{}')['returnValue']
    if level != '/Game/B1/Gym/Lvl_Gym_Core':
        raise RuntimeError('load Lvl_Gym_Core first, the editor has ' + level)
    execute_tool('editor_toolset.toolsets.object.ObjectTools.set_properties',
                 json.dumps({'instance': {'refPath': JEEP + '.MudDirt'}, 'values': json.dumps({'PreviewDirt': v})}))


def cap(p):
    x = {'location': {'x': p[0], 'y': p[1], 'z': p[2]}, 'rotation': {'pitch': p[3], 'yaw': p[4], 'roll': 0},
         'scale': {'x': 1, 'y': 1, 'z': 1}}
    a = {'gridSpacing': 0, 'gridExtent': 0, 'gridHeight': 0, 'maxLabelDistance': 0, 'classFilter': None, 'maxLabels': 0}
    return execute_tool('EditorToolset.EditorAppToolset.CaptureViewport',
                        json.dumps({'captureTransform': x, 'annotations': a}))['returnValue']['image']['data']


def run():
    out = {}
    for s in STAGES:
        set_preview(s)
        time.sleep(0.5)
        for name, pose in POSES.items():
            out['d%03d_%s' % (int(s * 100), name)] = cap(pose)
    set_preview(-1.0)
    return out
