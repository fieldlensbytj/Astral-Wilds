# Re-imports a species' Meshy FullSize GLB (art repo ArtSource/Meshy/Astrals/
# <Name>/FullSize/<Name>_FullSize.glb) over /Game/Astral/Species/<Name>/Model/,
# so the static display mesh and the glTF material/textures that
# wire_rigs.py re-parents the rigged Material_0 onto match a new model.
# (Added 2026-10-09 when TJ replaced the Cindrel and Ripplefin GLBs.)
# Close the editor first, then, before reimport_rig.py:
#
#   $env:ASTRAL_NAME="Cindrel"
#   & "...\UnrealEditor-Cmd.exe" <uproject> -run=pythonscript "-script=<this file>" -unattended -nopause -nosplash -NullRHI -stdout
#
# Like reimport_rig.py it exits non-zero from an Interchange Slate assert
# after the assets are saved (harmless); check the Model folder.
import unreal, os
n = os.environ["ASTRAL_NAME"]
eal = unreal.EditorAssetLibrary
model = "/Game/Astral/Species/%s/Model" % n
dest = "%s/%s_FullSize" % (model, n)
if eal.does_directory_exist(dest):
    eal.delete_directory(dest)
task = unreal.AssetImportTask()
task.set_editor_property("filename", r"C:\Users\camer\Astral Wilds\ArtSource\Meshy\Astrals\%s\FullSize\%s_FullSize.glb" % (n, n))
task.set_editor_property("destination_path", model)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
eal.save_directory(model, only_if_is_dirty=False, recursive=True)
# Interchange guesses texture types from the pixels: Ripplefin's mostly-blue
# base colour came in as a normal map (sRGB off, normal-map compression), so
# it rendered green-yellow in game. Force the base colour back to colour.
mat = eal.load_asset("%s/Materials/%s_FullSize_material_0" % (dest, n))
for p in mat.get_editor_property("texture_parameter_values"):
    t = p.get_editor_property("parameter_value")
    if t and str(p.get_editor_property("parameter_info").get_editor_property("name")) == "BaseColorTexture":
        t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
        t.set_editor_property("srgb", True)
        eal.save_loaded_asset(t, only_if_is_dirty=False)
