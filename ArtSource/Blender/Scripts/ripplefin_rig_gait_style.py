# Sets Ripplefin's gait style on Rigs/Ripplefin_Rigged.blend (armature properties read
# by quadruped_reanimate.py). Run astral_rig_quadruped.py first, then this,
# then reanimate:
#
#   blender -b --python ripplefin_rig_gait_style.py
#   blender -b --python quadruped_reanimate.py -- Ripplefin <out_dir>
#
# otter (reference: Otter on the move, youtube CYz6wMkkyrk): low and loping, the body rising and falling more than the deer.
import bpy
RIG = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs/Ripplefin_Rigged.blend"
STYLE = dict(lift_scale=1.1, hind_scale=0.85, hind_push=35.0, bob_scale=1.3, fit_stance=True)
bpy.ops.wm.open_mainfile(filepath=RIG)
arm = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE'][0]
for k, v in STYLE.items():
    arm[k] = v
bpy.ops.wm.save_as_mainfile(filepath=RIG)
print("gait style: %s" % STYLE)
