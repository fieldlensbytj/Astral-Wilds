# Sets Cindrel's gait style on Rigs/Cindrel_Rigged.blend (armature properties read
# by quadruped_reanimate.py). Run astral_rig_quadruped.py first, then this,
# then reanimate:
#
#   blender -b --python cindrel_rig_gait_style.py
#   blender -b --python quadruped_reanimate.py -- Cindrel <out_dir>
#
# red fox (reference: Red Fox Running Through The Field, youtube G3MAe18WPvg): light, quick, springy steps; hind stroke kept close to the fronts so the back legs do not splay.
import bpy
RIG = r"C:/Users/camer/Astral Wilds/ArtSource/Blender/Rigs/Cindrel_Rigged.blend"
STYLE = dict(lift_scale=1.25, hind_scale=0.85, hind_push=35.0, fit_stance=True)
bpy.ops.wm.open_mainfile(filepath=RIG)
arm = [o for o in bpy.context.scene.objects if o.type == 'ARMATURE'][0]
for k, v in STYLE.items():
    arm[k] = v
bpy.ops.wm.save_as_mainfile(filepath=RIG)
print("gait style: %s" % STYLE)
