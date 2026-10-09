# Wires each rigged Astral after reimport_rig.py: Material_0 -> the static
# glTF material (full PBR), species BP -> rigged mesh + Idle/Walk/Run clips,
# and Walk/Run authored speeds from quadruped_reanimate.py's speeds.txt
# (model m/s, model normalised to 1.9m tall, UE fits it to DisplayHeight).
#
#   $env:ASTRAL_RIGS="Glacielle,Mossling"
#   $env:ASTRAL_SPEEDS_DIR="<the out_dir given to quadruped_reanimate.py; holds <Name>\speeds.txt>"
#   & "...\UnrealEditor-Cmd.exe" <uproject> -run=pythonscript "-script=<this file>" -unattended -nopause -nosplash -NullRHI -stdout
#
# The summary is written to $ASTRAL_SPEEDS_DIR\wire_rigs.txt.
import unreal, os
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
speeds_dir = os.environ["ASTRAL_SPEEDS_DIR"]
out = []
for n in os.environ.get("ASTRAL_RIGS", "Glacielle,Mossling").split(","):
    R = "/Game/Astral/Species/%s/Rigged/" % n
    mat = eal.load_asset(R + "Material_0")
    static_mi = eal.load_asset("/Game/Astral/Species/%s/Model/%s_FullSize/Materials/%s_FullSize_material_0" % (n, n, n))
    if isinstance(mat, unreal.MaterialInstanceConstant):
        mel.set_material_instance_parent(mat, static_mi)
        mat.set_editor_property("texture_parameter_values", [])
        mel.update_material_instance(mat)
        eal.save_loaded_asset(mat, False)
    bp = eal.load_asset("/Game/Astral/Species/%s/BP_Species_%s" % (n, n))
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    cdo.set_editor_property("display_mesh", eal.load_asset(R + "%s_Rigged" % n))
    anims = {}
    for clip in ("Idle", "Walk", "Run"):
        anims[clip] = eal.load_asset(R + "%s_Rigged%sRig_%s" % (n, n, clip))
        cdo.set_editor_property(clip.lower() + "_anim", anims[clip])
    # Flying species (Stormrook) also have Fly / Glide / Flare clips.
    for clip in ("Fly", "Glide", "Flare"):
        a = eal.load_asset(R + "%s_Rigged%sRig_%s" % (n, n, clip)) if eal.does_asset_exist(R + "%s_Rigged%sRig_%s" % (n, n, clip)) else None
        if a:
            anims[clip] = a
            cdo.set_editor_property(clip.lower() + "_anim", a)
    cdo.set_editor_property("display_yaw_offset", -90.0)
    speeds = dict(l.split() for l in open(os.path.join(speeds_dir, n, "speeds.txt")).read().splitlines())
    k = cdo.get_editor_property("display_height") / 190.0
    cdo.set_editor_property("walk_anim_speed", round(float(speeds["Walk"]) * 100.0 * k, 1))
    cdo.set_editor_property("run_anim_speed", round(float(speeds["Run"]) * 100.0 * k, 1))
    cdo.modify()
    eal.save_loaded_asset(bp, only_if_is_dirty=False)
    out.append("%s: mat parent=%s walk=%s run=%s lengths=%s" % (n, mat.get_editor_property("parent").get_name(),
        cdo.get_editor_property("walk_anim_speed"), cdo.get_editor_property("run_anim_speed"),
        {c: round(a.get_editor_property("sequence_length"), 3) if a else None for c, a in anims.items()}))
open(os.path.join(speeds_dir, "wire_rigs.txt"), "w").write("\n".join(out))
