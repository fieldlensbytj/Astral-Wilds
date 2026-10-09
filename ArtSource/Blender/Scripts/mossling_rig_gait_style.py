# Sets Mossling's gait style on Rigs/Mossling_Rigged.blend (armature
# properties read by quadruped_reanimate.py). Run it, then reanimate:
#
#   blender -b --python mossling_rig_gait_style.py
#   blender -b --python quadruped_reanimate.py -- Mossling <out_dir>
#
# Why (TJ, 2026-10-08: give the others Glacielle's gait polish): Mossling's
# model stands with its hind paws ~12cm outside each hip, as Glacielle's did
# (TJ: "massive spacing in the back legs"), and its Run hind stroke was 1.58x
# the fronts'. A small woodland deer (reference: roe deer) trots light and
# springy with its hind feet tracking under the hips.
import bpy
RIG = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs/Mossling_Rigged.blend"
STYLE = dict(hind_track=0.36,    # hind feet just outside the hips (model: 0.54m apart, hips 0.30)
             hind_scale=0.8,     # Run hind stroke 0.80 -> 0.64m (fronts 0.51m)
             hind_push=32.0,     # gentler toe-off than the default 45 deg
             lift_scale=1.3)     # lighter, springier steps
bpy.ops.wm.open_mainfile(filepath=RIG)
arm = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE'][0]
for k, v in STYLE.items():
    arm[k] = v
bpy.ops.wm.save_as_mainfile(filepath=RIG)
print("gait style: %s" % STYLE)
