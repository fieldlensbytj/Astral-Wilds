# Sets Glacielle's gait style on Rigs/Glacielle_Rigged.blend (armature
# properties read by quadruped_reanimate.py). Run it, then reanimate:
#
#   blender -b --python glacielle_rig_gait_style.py
#   blender -b --python quadruped_reanimate.py -- Glacielle <out_dir>
#
# Why (TJ, 2026-10-08: Glacielle "doesn't seem as natural as Ironbur"; legs
# look stiff/robotic): her front legs are modelled dead straight (rest length
# == leg length), and her strides ran to 98.5% of reach, so the legs locked
# straight in stance and at both ends of every step (bend down to 5 deg),
# with a 7cm foot lift on a tall, deer-like body. A slight crouch keeps the
# joints soft, the stride stays inside 92% of reach, and the feet lift higher.
import bpy
RIG = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs/Glacielle_Rigged.blend"
STYLE = dict(crouch=0.04, reach_margin=0.92, lift_scale=1.6, fit_stance=True,
             run_style="reindeer",   # TJ: she is a reindeer; see quadruped_reanimate.py
             hind_scale=0.7, hind_push=28.0,   # TJ: back legs too far apart; "refined, poised", a queen's elegance
             hind_track=0.32)      # hind feet just outside the hips (model: 0.54m apart; 0.26 was a touch narrow)
bpy.ops.wm.open_mainfile(filepath=RIG)
arm = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE'][0]
for k, v in STYLE.items():
    arm[k] = v
bpy.ops.wm.save_as_mainfile(filepath=RIG)
print("gait style: %s" % STYLE)
