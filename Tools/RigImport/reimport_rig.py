# Imports ArtSource/Blender/Rigs/<Name>_Rigged.fbx (art repo) fresh into
# /Game/Astral/Species/<Name>/Rigged, deleting the old folder first: a
# reimport over existing assets crashes before the animations save.
# Close the editor first. Run once per Astral, then run wire_rigs.py:
#
#   $env:ASTRAL_NAME="Glacielle"
#   & "D:\Games\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" <uproject> -run=pythonscript "-script=<this file>" -unattended -nopause -nosplash -NullRHI -stdout
#
# It exits non-zero (an Interchange Slate assert after the import). That's
# harmless, because the assets are already saved.
import unreal, os
n = os.environ["ASTRAL_NAME"]
dest = "/Game/Astral/Species/%s/Rigged" % n
if unreal.EditorAssetLibrary.does_directory_exist(dest):
    unreal.EditorAssetLibrary.delete_directory(dest)
task = unreal.AssetImportTask()
task.set_editor_property("filename", r"C:\Users\camer\Astral Wilds\ArtSource\Blender\Rigs\%s_Rigged.fbx" % n)
task.set_editor_property("destination_path", dest)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
unreal.EditorAssetLibrary.save_directory(dest, only_if_is_dirty=False, recursive=True)
