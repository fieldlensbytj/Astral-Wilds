# Sets each species' turning feel (and Stormrook's flight) on its
# BP_Species_<Name> class defaults. The values come from the real-animal
# references in the art repo's Docs/Design/TurnReference.md. Close the editor
# first, then:
#
#   & "...\UnrealEditor-Cmd.exe" <uproject> -run=pythonscript "-script=<this file>" -unattended -nopause -nosplash -NullRHI -stdout
#
# The summary is written to Saved/SpeciesTuning/set_movement.txt.
import unreal, os
eal = unreal.EditorAssetLibrary

# name: (turn_acceleration cm/s^2, max_turn_lean deg, head_lead_max deg, can_fly)
SPECIES = {
    # Fox: light and agile, cuts sharp, head leads hard.
    "Cindrel":   (1200.0, 16.0, 40.0, False),
    # Roe deer: nimble, herds curve in quick arcs.
    "Mossling":  (1000.0, 12.0, 35.0, False),
    # Reindeer: the poised, extended-trot arcs TJ approved (2026-10-08).
    "Glacielle": (800.0, 14.0, 35.0, False),
    # Wild boar: heavy and stiff-bodied, swings wide, barely leans; the
    # whole body turns rather than the head.
    "Ironbur":   (550.0, 7.0, 20.0, False),
    # Otter: sinuous and quick, head first.
    "Ripplefin": (1000.0, 10.0, 40.0, False),
    # Raptor on the ground: a few deliberate steps. In the air: see Flight.
    "Stormrook": (700.0, 8.0, 35.0, True),
}

out = []
for name, (accel, lean, lead, fly) in SPECIES.items():
    bp = eal.load_asset("/Game/Astral/Species/%s/BP_Species_%s" % (name, name))
    if not bp:
        out.append("%s: BP not found" % name)
        continue
    cdo = unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))
    cdo.set_editor_property("turn_acceleration", accel)
    cdo.set_editor_property("max_turn_lean", lean)
    cdo.set_editor_property("head_lead_max", lead)
    cdo.set_editor_property("can_fly", fly)
    cdo.modify()
    eal.save_loaded_asset(bp, only_if_is_dirty=False)
    out.append("%s: turn %s, lean %s, head lead %s, flies %s" % (name, cdo.get_editor_property("turn_acceleration"),
        cdo.get_editor_property("max_turn_lean"), cdo.get_editor_property("head_lead_max"), cdo.get_editor_property("can_fly")))

d = os.path.join(unreal.Paths.project_saved_dir(), "SpeciesTuning")
os.makedirs(d, exist_ok=True)
open(os.path.join(d, "set_movement.txt"), "w").write("\n".join(out))
