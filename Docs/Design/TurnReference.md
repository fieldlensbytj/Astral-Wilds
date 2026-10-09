# Turning reference: how animals and runners turn at speed

Requested by TJ on 2026-10-08: "The turns need to be more fluid, more like a baseball player rounding a base or a gazelle turning as it's being chased." These notes come from frame-by-frame study of the videos below. The stills are in `Reference/Turning/`, and the file names carry each video's ID. They also record what changed in the game as a result.

## Sources studied

| Clip | What it shows | Still |
|---|---|---|
| [Cheetah changing directions rapidly while hunting a gazelle](https://www.youtube.com/watch?v=PPMBitY0ixE), 0:01-0:05 | A Thomson's gazelle sees the cheetah, wheels away and accelerates out of the turn | `gazelle_cut_PPMBitY0ixE.jpg` |
| [Cheetah Chases Gazelle, BBC Studios](https://www.youtube.com/watch?v=DRxsP2YMaOI), 1:02-1:07 | Gazelles cutting side-on, then a head-on slow-motion cheetah banking hard through a turn in a cloud of dust | `gazelle_cheetah_bank_DRxsP2YMaOI.jpg` |
| [Cheetah hunts a gazelle in slow motion, Rate My Science](https://www.youtube.com/watch?v=EEyGz9grA5k), 1:12-1:33 | Repeated zig-zag cuts by the prey, with the cheetah's spine curving and its tail swinging out through each turn | `cheetah_tail_rudder_EEyGz9grA5k.jpg` |
| [MASTER the Art of Rounding First Base, Figure It Out Baseball](https://www.youtube.com/watch?v=GVycFma-Rbk), 1:12-1:20 | A college runner's banana route, bowing out before the bag and leaning in to round it | `baserunner_round_GVycFma-Rbk.jpg` |
| [Reindeer running near North Pole, Alaska](https://www.youtube.com/watch?v=OoqfsSWDMBs), 0:12-0:14 | Glacielle's own gait reference (TJ's pick): the extended trot | `reindeer_extended_trot_OoqfsSWDMBs.jpg` |

Background reading: the base-running "banana route" (Morgan, Williams College, [The Fastest Path Around the Bases](https://communications.williams.edu/news-releases/the-fastest-path-around-the-bases-world-series-take-notice/)). The sharp-cornered straight-line path is the shortest but one of the slowest, because a runner has to brake for the corner. A curved path that keeps the speed is faster. On quadrupeds, [Speed-Dependent Turning Strategies in Quadrupedal Locomotion](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC12871115/) describes three ways of steering: body bending, lateral force and lateral limb shifting. Which ones an animal uses depends on its speed.

## What the footage shows

1. **The path curves; the body never pivots on the spot at speed.** The gazelle's escape is a curve several body lengths wide, not a corner. The runner bows out before the bag ("banana route"), so they arrive already angled toward second base and carry their speed round. Pivoting on the spot only happens from a standstill or a walk.
2. **Speed is kept through the turn.** Neither the gazelle nor the runner brakes into the curve. The gazelle even accelerates out of it. A turn costs a little speed at most, never a stop and restart.
3. **The body faces along the direction of travel.** Throughout the curve the spine points the way the animal is actually moving. It doesn't crab sideways toward the goal.
4. **It banks into the curve.** The head-on cheetah shot shows the clearest bank: the whole body tilts toward the inside of the turn while the feet push out to the side. The gazelle and the runner lean in visibly too. The faster and tighter the turn, the bigger the lean, and it unwinds as the path straightens.
5. **The head leads.** The gazelle's head and horns swing to the new direction before its body does, and the runner looks toward the next base before the turn. The eyes pick the line and the body follows.
6. **The spine curves into the arc and the tail counter-swings.** Through each cut the cheetah's spine bends into a C along the curve and its long tail swings out. That's the counterweight and rudder action described in the BBC piece.
7. **The reindeer's look is a level, gliding extended trot.** Its long, low strides reach well forward, and the body barely bobs. Turns should keep that glide: no hop, no stall.

## What changed in the game (2026-10-08)

Before this change, a wild Astral at a path corner aimed its steering straight at the next point. The engine's ground friction then swung its velocity round at about 500 deg/s, cutting across the corner. That chord cost about 14% of its speed every frame, so it dropped to a walk, pivoted at the 240 deg/s rotation cap, and set off again. Its body faced the steering direction rather than the way it was moving, so it crabbed. It was a stop-spin-go, the opposite of points 1-3.

| Reference point | Implementation (Unreal repo) |
|---|---|
| 1, 2: arc at speed, keep the speed | `UAstralMovementComponent` (new, used by every `AAstralCharacter`). How fast the heading can turn is capped by a maximum sideways acceleration, `MaxTurnAcceleration` = 800 cm/s^2. That's about a 2.5m radius (about 100 deg/s) at the 450 cm/s run, while a 140 cm/s walk can still turn nearly on the spot. Each frame the steering is aimed only as far off the heading as friction needs to turn at that rate (about 13 deg at a run, `SteerAngleForStep`), so the velocity swings round without bleeding speed. Below `ArcMinSpeed` (100 cm/s) nothing is limited, and the limits ease in up to `ArcFullSpeed` (350 cm/s). |
| 3: face the direction of travel | The same component faces the velocity once moving (`ComputeOrientToMovementRotation`). From rest it still turns toward its goal as before. |
| 4: bank | `AAstralCharacter::UpdateTurnLean` now banks by the sideways acceleration (speed x turn rate). It uses half the physical bank angle, for a poised look rather than a motorbike lean, and is capped at `MaxTurnLean` = 14 deg (was 10). A slow pivot barely leans; a fast arc leans fully. |
| 5: head leads | `UAstralLocomotionAnimInstance` turns the neck (40%) and head (60%) up to 35 deg toward where the Astral is steering, ahead of the body, eased in and out (`HeadLead`). |
| 6: spine and tail | Already in place: the body bend into turns (`TurnBend`: spine, chest, neck and head into the turn, tail to the other side). |

Measured on the new `Astral.MotionCapture Glacielle 20 Circuit` course, which loops the flat ground ring round `Lvl_ThirdPerson`'s central block at 450 cm/s. Before the change (`CircuitPivot` runs the same course without the new turning), every corner slowed her, to 66-72 cm/s at the sharpest and 350-370 at the mildest, with the facing spinning at the 240 deg/s cap. After it, she held 450 cm/s and her heading swung a steady 102 deg/s with the body aligned to her travel (`glacielle_arc_ingame.jpg`). The suite (62 tests, with the new `AstralWilds.Movement.SteerClamp`) and the bot playtest pass.

## Tuning knobs

- Wider, lazier arcs: lower `MaxTurnAcceleration` (600 gives about 3.4m at a run). Snappier cuts: raise it (1200 gives about 1.7m).
- More or less lean: `MaxTurnLean` on the Astral, plus the 0.5 bank factor in `UpdateTurnLean`.
- Head lead: the 35 deg cap and the 0.4/0.6 neck/head shares in `AstralLocomotionAnimInstance.cpp`.

## Per-species turning (2026-10-08, TJ: "do that for all of them")

Each Astral's turn feel now comes from its real-animal model, set on its species asset (`UAstralSpeciesData` `TurnAcceleration`, `MaxTurnLean`, `HeadLeadMax`; applied at spawn by `AAstralCharacter::ApplySpeciesMovement`; values written by `Tools/SpeciesTuning/set_movement.py` in the Unreal repo).

| Astral | Model | Reference | Turn accel (tightest arc at a 450 run) | Lean | Head lead |
|---|---|---|---|---|---|
| Cindrel | Red fox | [Red fox running through a field](https://www.youtube.com/watch?v=G3MAe18WPvg) (`fox_run_G3MAe18WPvg.jpg`): light, low, quick direction changes, nose first | 1200 (1.7m) | 16 | 40 |
| Mossling | Roe deer | [Running roe deer](https://www.youtube.com/watch?v=lSQUPkHcJHg): small herd deer swing round in quick, springy arcs | 1000 (2.0m) | 12 | 35 |
| Glacielle | Reindeer | as above (poised extended trot) | 800 (2.5m) | 14 | 35 |
| Ironbur | Wild boar | [Wild boar herd, Wilpattu NP](https://www.youtube.com/watch?v=L7WHGmwdKW8) (`boar_herd_L7WHGmwdKW8.jpg`), [warthog fleeing lions, BBC](https://www.youtube.com/watch?v=TYe-Au6HXD4) (`warthog_chase_TYe-Au6HXD4.jpg`): a stiff, heavy, level body; short quick steps; turns wide and with the whole body, barely leaning, head locked to the body | 550 (3.7m) | 7 | 20 |
| Ripplefin | Otter | [Otter on the move](https://www.youtube.com/watch?v=CYz6wMkkyrk) (`otter_CYz6wMkkyrk.jpg`): sinuous, low and quick; the head goes first and the body flows after | 1000 (2.0m) | 10 | 40 |
| Stormrook (on the ground) | Raptor | a few deliberate steps between flights | 700 | 8 | 35 |

**Gait polish for the other rigs** (same pipeline as Glacielle; per-rig scripts `mossling_rig_gait_style.py` / `ironbur_rig_gait_style.py` in `ArtSource/Blender/Scripts`):
- **Mossling** had the same wide-hipped hind stance as Glacielle (hind paws 0.54m apart, ~12cm outside each hip). Now `hind_track` 0.36, the Run hind stroke shortened 0.80 -> 0.59m (`hind_scale` 0.8, `hind_push` 32), and springier steps (`lift_scale` 1.3).
- **Ironbur** (already TJ's favourite, so a light touch): a boar's trot is short, quick and choppy with a level body. Strides are 20% shorter (`stride_scale` 0.8, a new option: same ground speed, so it steps faster), the hind stroke is 0.98 -> 0.71m, and the body bob is cut to 60% (`bob_scale`, also new).

## Flight: Stormrook (2026-10-08, TJ: "stormrook is a flyer so it needs flying mechanics"; chose "mostly airborne, lands to rest")

References: [bald eagle landing, 1000fps](https://www.youtube.com/watch?v=h8GgH1plf2E) (`eagle_landing_h8GgH1plf2E.jpg`), [raven taking off](https://www.youtube.com/watch?v=9BhV0h5I1hQ) (`raven_takeoff_9BhV0h5I1hQ.jpg`), [Harris hawk flying free](https://www.youtube.com/watch?v=8J3FoflQKVo) (`harris_hawk_8J3FoflQKVo.jpg`).

What they show:
1. **Take-off** is a crouch-and-leap with big, fast wingbeats, then a shallow climb away. It is not a vertical lift.
2. **Soaring** is wide, steady circles with the wings still and the body banked hard into the turn, held there. The height rises and falls gently.
3. **Landing** is a long, descending glide on a shallow slope. Over the last few metres the bird flares: body pitched up, wings spread and beating to brake, legs reaching forward, speed bleeding off. Then it drops the last bit onto its feet.
4. **Turns in the air** are always arcs; a bird never pivots in flight.

In the game (Unreal repo):
- `UAstralSpeciesData::bCanFly` and `FAstralFlightTuning`: cruise height 6.5m, soar radius 9m, speeds 520 cruise / 450 chase (below the Mage's 500) / 650 flee, flying turn acceleration 700, bank up to 40 deg, 18-30s aloft and 8-14s resting.
- `AAstralWildlifeController` flight phases **Grounded -> TakingOff -> Airborne -> Landing -> Grounded** (`ChooseFlightPhase`, a pure rule with tests). It layers under the usual wander/chase/flee modes: a chase, flee or trip home gets it up at once, a wandering flyer soars its air time then lands near home to rest, and a Receptive one lands and stays down so it can be bonded. Steering (`SteerFlight`) runs every frame: climb out, soaring circles round home (`OrbitTarget`), swooping low over the player when chasing, climbing away when fleeing, and the landing glide down a ~24 deg slope (`GlideSlopeHeight`), slowing into the flare. It pulls up and heads home if something solid is ahead.
- `UAstralMovementComponent` carves arcs in the air too (`MaxFlightTurnAcceleration`).
- `AAstralCharacter::UpdateFlightPose`: the full physical bank angle in the air (up to 40 deg, versus half on the ground), pitch following the climb or dive, a nose-up flare when slow, and flapping (quick body heave and rock while climbing or slow) versus gliding (a gentle rise and fall).
- Judge it with `Astral.MotionCapture Stormrook 40 Alone` (the full take-off, soar and landing cycle) or `Astral.MotionCapture Stormrook 15` (it chases the Mage). Stills: `stormrook_takeoff_ingame.jpg`, `stormrook_soar_ingame.jpg`, `stormrook_landing_ingame.jpg`.

**Wings (rigged 2026-10-08, TJ: "the wings need to flap"):** `stormrook_rig_prep.py` then `stormrook_rig_rig_stormrook.py -- <out_dir>` (art repo) build a 22-bone bird rig: body, neck, head and tail bones named like the quadrupeds (so turn bend and head lead work), bird legs, and a three-bone wing per side (upper arm, forearm, hand). The Meshy model is ~4900 separate feathers, so each feather is skinned rigidly from its centre and belongs to either a wing or the body, never both (no shearing or stretching). The mesh is decimated to 100k triangles: at 24k (~5 per feather) the feathers shrank apart and the sky showed through. Feathers round each shoulder blend from the chest to the upper arm, so the wing root stretches instead of tearing away. The wings lie flat like a real bird's: aiming a bone only sets where it points, not its twist, so each wing segment is also twisted about its own axis until its feather surface (measured from the mesh; the upper arm shares the forearm's) faces up, tilted by the stroke and a small angle of attack. **Flare** (braking, eagle landing reference): the body pitches ~38 deg nose-up, the wings sweep forward and stand at ~55 deg to the air like air brakes with slow deep beats, the tail fans down and the feet reach forward; the anim instance blends to it whenever it loses speed in the air or flies slow and level. Clips: Idle (perched, quick head turns, a wing rouse), Walk/Run (raptor stride with head-bob), **Fly** (powered downstroke with the wing spread and the tip lagging, then a folded upstroke, ~1.4 beats/s, faster when climbing) and **Glide** (wings held out with a slight dihedral). The flight clips level the upright body, tuck the legs back and stretch the head forward. In game, `UAstralLocomotionAnimInstance` crossfades ground clips to Fly/Glide when airborne (flapping in raven-style bursts, ~4s of wingbeats then a ~1.2s glide, and always on take-off, climbing and the slow landing flare; longer glides only in steep dives. It used to glide whenever cruising level, which TJ saw as "the wings do not flap at all"), and the character pitches the body with the climb/dive and flares nose-up when slow. Stills: `stormrook_rig_clips.jpg`, `stormrook_rigged_takeoff_ingame.jpg`, `stormrook_rigged_flap_ingame.jpg`, `stormrook_rigged_glide_ingame.jpg`.

## Not done yet (candidates)

- A small speed dip with a push-off out of very sharp cuts (the gazelle's burst out of the turn).
- An actual tail rudder swing that scales with turn rate. Today the tail only follows the body bend.
