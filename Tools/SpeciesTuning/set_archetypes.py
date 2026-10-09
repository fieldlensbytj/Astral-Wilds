# Sets each species' AI archetype on its BP_Species_<Name> class defaults
# (how a wild one reacts to the Mage; see AAstralWildlifeController). Close
# the editor first, then:
#
#   & "...\UnrealEditor-Cmd.exe" <uproject> -run=pythonscript "-script=<this file>" -unattended -nopause -nosplash -NullRHI -stdout
#
# The summary is written to Saved/SpeciesTuning/set_archetypes.txt.
import unreal, os
eal = unreal.EditorAssetLibrary

SPECIES = {
    # TJ, 2026-10-09: "cindrel to be agressive like stormrook since its a fire base".
    "Cindrel":   unreal.AstralAIArchetype.AGGRESSIVE,
    "Mossling":  unreal.AstralAIArchetype.DOCILE,
    "Ironbur":   unreal.AstralAIArchetype.TERRITORIAL,
    # TJ, 2026-10-09: "weary but not as skiddish as glacielle".
    "Ripplefin": unreal.AstralAIArchetype.WARY,
    "Stormrook": unreal.AstralAIArchetype.AGGRESSIVE,
    "Glacielle": unreal.AstralAIArchetype.SKITTISH,
}

out = []
for name, archetype in SPECIES.items():
    bp = eal.load_asset("/Game/Astral/Species/%s/BP_Species_%s" % (name, name))
    if not bp:
        out.append("%s: BP not found" % name)
        continue
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    cdo.set_editor_property("ai_archetype", archetype)
    cdo.modify()
    eal.save_loaded_asset(bp, only_if_is_dirty=False)
    out.append("%s: %s" % (name, cdo.get_editor_property("ai_archetype")))

d = os.path.join(unreal.Paths.project_saved_dir(), "SpeciesTuning")
os.makedirs(d, exist_ok=True)
open(os.path.join(d, "set_archetypes.txt"), "w").write("\n".join(out))
