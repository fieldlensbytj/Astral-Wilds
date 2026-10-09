# Sets Ironbur's gait style on Rigs/Ironbur_Rigged.blend (armature
# properties read by quadruped_reanimate.py, alongside plant_fr/fit_stance
# from ironbur_rig_rig_ironbur.py). Run it, then reanimate:
#
#   blender -b --python ironbur_rig_gait_style.py
#   blender -b --python quadruped_reanimate.py -- Ironbur <out_dir>
#
# Why (TJ, 2026-10-08: give the others Glacielle's gait polish; Ironbur was
# already "the best so far", so this is light): a wild boar's trot and run
# (references: wild boar herd, Wilpattu NP, youtube L7WHGmwdKW8; warthog
# chased by lions, BBC, TYe-Au6HXD4) are short, quick, choppy strides with a
# level, stiff body - not long reaching ones. Shorter strides at the same
# ground speed make the engine step faster.
import bpy
RIG = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs/Ironbur_Rigged.blend"
STYLE = dict(stride_scale=0.8,   # Walk/Run strides 20% shorter (quicker steps)
             hind_scale=0.9,     # Run hind stroke 0.98 -> ~0.71m (fronts ~0.51m)
             bob_scale=0.6)      # a level, heavy body
bpy.ops.wm.open_mainfile(filepath=RIG)
arm = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE'][0]
for k, v in STYLE.items():
    arm[k] = v
bpy.ops.wm.save_as_mainfile(filepath=RIG)
print("gait style: %s" % STYLE)
