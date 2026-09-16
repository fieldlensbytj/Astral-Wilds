# Astral Wilds Provenance

## Design decision: corrected Astral Battle Format

Recorded 2026-09-16 from the project owner's corrected specification.

The canonical battle rule is 2v2. Each trainer may own up to six Astrals, with two active simultaneously and four available for switching. The maximum active battlefield population is four Astrals total. A seventh captured Astral goes to a separate reserve collection. Opponent parties may contain up to six Astrals, while wild encounters may use two when appropriate.

The implementation intentionally stops at data structures, slot ownership, switching rules, target-scope descriptors, and EditMode validation. No final combat timing model, abilities, or balance values are established by this change.

## Asset provenance

The crashed corvette remains the approved detailed Meshy-derived design, conservatively cleaned and prepared in Blender. The current implementation does not regenerate or simplify that asset.

## Established Astral visual references

The primary source library is `C:\Users\camer\OneDrive\Documents\Astral Wilds\Astral_World_Astral_Images`. The selected playable-demo references are the three Main Astrals sheets copied, without alteration, to `Assets/Art/References/Astrals/MainAstrals/`. They establish the six demo designs Cindrel, Mossling, Ripplefin, Stormrook, Ironbur, and Glacielle. See `Docs/Design/AstralReferenceInventory.md` for the complete inventory, source paths, identity confidence, and reserved references.

No matching creature models or textures were present in the project during the audit. No model generation, texturing service, external upload, or Meshy credit spend was performed. Creature visuals therefore remain reference/portrait assets only; any future 3D assets must preserve these silhouettes, proportions, colors, markings, and identity.
