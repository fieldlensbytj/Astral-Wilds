# Cindrel 3D Asset Production Specification

Status: normalized, round-trip validated, imported into Unity, and packaged as a reusable prefab on 2026-09-16. Final texturing, retopology for deformation, rigging, animation, prompt recovery, and provider-license confirmation remain open.

## Stable identity

- Asset name: `Cindrel`
- Version: `Production_v01`
- Intended use: Unity 6 desktop prototype, close-to-medium-distance party/battle creature
- Visual authority: `Assets/Art/References/Astrals/MainAstrals/01_Main_Astrals_Cindrel_Mossling.png`
- Source provider artifact: `ArtSource/Meshy/Astrals/Cindrel/Preview01/Cindrel_Preview01.glb`
- Source prompt: not recorded in the available repository or Blender review scene
- License status: pending confirmation against the user's Meshy/provider terms; no canonical redistribution package may be declared until resolved

## Normalization policy

- Preserve the source GLB and temporary review `.blend` byte-for-byte.
- Refuse to overwrite any existing production output.
- Reduce the 975,404-triangle watertight source to a 35,000-triangle LOD0 budget.
- Preserve the source silhouette through collapse decimation; do not claim animation-ready topology.
- Normalize to 1.25 meters tall, centered on X/Y, grounded at Z=0, with the object origin at the ground pivot.
- Recalculate outward normals, smooth-shade the organic surface, and generate one non-overlapping smart-projected UV set.
- Apply a single warm ember clay material for review only. Final identity texturing remains a separate authored pass.
- Export a selected-mesh-only FBX using Unity axes (`-Z` forward, `Y` up) with transforms and units applied.

## Exact invocation and outputs

The invocation must be run from the repository root with Blender 5.2.1 LTS. It writes only these new files:

- `ArtSource/Blender/Astral_Cindrel_Production_v01.blend`
- `ArtSource/Blender/Previews/Cindrel_Production_v01_ThreeQuarter.png`
- `Assets/Art/Astrals/Cindrel/Models/Cindrel_Production_v01.fbx`

The script exits instead of overwriting if any output already exists.

## Acceptance gates

- Output triangle count is at or below 36,000.
- Output remains closed/manifold with no loose vertices or edges.
- Dimensions are approximately 1.25 meters tall and the minimum Z is zero.
- At least one UV layer and one review material exist.
- FBX round-trip inspection agrees with the production `.blend` geometry and dimensions.
- The normalized preview retains Cindrel's recognizable fox-like head, large ears, layered ember-fur silhouette, four-legged stance, and curled/flame-like tail.
- Unity import is evidenced separately; Blender normalization alone is not a Unity runtime validation.

## Recorded results

- Source SHA-256: `dd6b0d6472ecd1914de245a01dc36e846eef8f7ff2e1b6370f44e3b0bd36a0f8`
- Production Blender SHA-256: `6eee88b7bb391a4e510b325242311e3714fbb7df96ffb6b05fd849b53e81d962`
- Production FBX SHA-256: `2724c9b434890278b643dfc6eb8188526ae10204e30d276bf6b52610d6cf6fa2`
- Blender preview SHA-256: `51a5bc66843e806c955200f759452e2c3728d4ea7bf0184a8020b14fc693cafc`
- Blender 5.2.1 result: 17,492 vertices, 35,000 triangles, 0 boundary edges, 0 non-manifold edges, 0 loose vertices/edges, one `UVMap`, one review material.
- Dimensions: 1.087981 m x 1.065191 m x 1.25 m; grounded at Z=0 with identity object transform.
- FBX round trip reproduced the same topology, dimensions, UV set, material, identity transform, and custom provenance properties.
- Unity 6000.6.0f1 import: 35,997 imported vertices (expected vertex splitting at UV/normal boundaries), 35,000 triangles, imported normals, Mikk tangents, animation disabled, read/write disabled, mesh optimization enabled, and no compression.
- Unity prefab: `Assets/Art/Astrals/Cindrel/Prefabs/Cindrel_Production_v01.prefab`, with one renderer using `Cindrel_EmberClay.mat` and a capsule collider sized from rendered bounds.
- Unity validation scene: `Assets/Scenes/AssetValidation/CindrelValidation.unity`; deliberately excluded from Build Settings.
- `Assets/Astral.unity` was restored as the active, clean scene after validation. No gameplay-scene disk change was made.

## Remaining gates

- The collapse-decimated topology is suitable for a static prototype visual, but it is not claimed to be deformation-ready. A proper quad retopology should precede rigging.
- The ember-clay material is a readable placeholder, not the approved multicolor Cindrel texture.
- No skeleton, skin weights, clips, animator controller, LOD chain, or gameplay placement has been produced yet.
- The original Meshy prompt was not recoverable from the source GLB, Blender review scene, repository, or live browser state.
- Provider license confirmation is still required before declaring a canonical redistributable package.
- No paid provider call or credit spend was performed in this pass.
