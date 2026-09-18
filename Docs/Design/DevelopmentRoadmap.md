# Astral Wilds: Fresh-Start Development Roadmap

# Astral Wilds: Fresh-Start Development Roadmap

Sep 18, 2026 · @Someone

## Guiding principle

On a solo project, the biggest risk was never "can I write this feature" — it's spending months building systems before anyone, including you, has actually played a single fun minute of the game. So the order below is built around one rule: get to something pressable, on a real controller and keyboard, as fast as possible, then only add scope once what exists is already fun. Everything else — which system gets built first, how big the first playable slice is, when content gets added — follows from that one rule.

## Phase 0 — Foundation

**Before a single line of gameplay code:**

Pick one engine and commit. Starting fresh means picking Unreal (or whichever engine) once, setting up one clean project, and not running a parallel version in a second engine at the same time. Two versions of the same game invites confusion about which one is "real" and doubles every future decision.

Get version control working and verified end-to-end on day one — not "it's set up," but "I've made a real commit and confirmed it's actually there." If large binary assets (3D models, source art) are ever going to be part of the project, decide on Git LFS (or an equivalent) now, before the first large file ever gets added, rather than bolting it on after files are already committed the wrong way.

Confirm the project actually builds, from absolute zero, before writing anything custom. Create the project, build it once with no changes, and make sure that succeeds. This sounds trivial but catches environment problems (missing compiler components, broken default plugins) while there's zero custom code to blame, instead of discovering them later and wasting time wondering whether it's your code or the environment.

Set up basic input for keyboard+mouse and controller together, immediately, using whatever starter template the engine provides. Don't build custom movement/camera code from scratch on day one — use the engine's own working example as a base and only customize once something else needs it. The goal here is just "a character that can walk and look around, provably working on both input methods," as fast as possible.

## Phase 1 — Prove the core feel in isolation

Every game has one mechanic the player will perform thousands of times more than any other. For this game, that's the bonding/capture interaction — not combat, not menus, not story. Build that mechanic by itself, in an empty test level, before anything else exists around it:

Build the bonding minigame as a standalone system with no dependency on a real Astral, a real world, or real art — just a placeholder shape to bond with and the actual moment-to-moment interaction (tracking, holding, timed responses, a stability meter that fills through real play).

Get it working on keyboard+mouse and controller both, in that same empty test level, before building anything else.

Play it. A lot. Tune the numbers — how fast the target moves, how forgiving the timing is, how quickly the meter fills — until it's actually satisfying by itself, with nothing else around it to distract from how it feels.

The reason to do this before combat, before the world, before anything: if the single most-repeated interaction in the whole game doesn't feel good, no amount of content built on top of it will fix that, and it's far cheaper to find that out with a placeholder shape in an empty room than after building real Astrals, real levels, and real art around a mechanic that turns out to be tedious.

## Phase 2 — One tiny, real, playable loop

Once the bonding feel is proven, build the smallest possible version of the actual game loop — not a demo of one system, a genuine (if tiny) slice of the real thing:

Battle logic next, as pure rules first. Build the 2-versus-2 battle math (attack, a special move, guarding, the opposing side's response) as logic that can be checked and verified on its own, before worrying about how it looks. Getting the numbers and rules right first, separately from presentation, makes it far easier to catch a math or logic mistake — it's much harder to spot a damage formula bug once it's buried inside animations and UI.

Minimal UI immediately after — HP bars, a stability meter for bonding, a simple round-summary readout. Not polished, just enough to actually see what's happening. A game that works internally but shows nothing on screen can't really be evaluated.

One real wild creature, start to finish, in one small test area: it exists in the world, has its own difficulty for bonding, becomes bondable under some real (even simple) condition, and successfully joins a party you can see. This is the smallest end-to-end loop: explore → find → bond → have a party member. Get this working before adding a second creature.

Then one real fight, start to finish: encounter an opponent, fight using the battle logic from step 1, win or lose, and return to normal play. Also the smallest possible end-to-end version — one opponent, not a whole roster.

At the end of Phase 2 the entire game is maybe one test room, one creature, one opponent — but everything in it is real and playable by hand, not a mockup.

## Phase 3 — Playtest and tune before adding scope

This is the phase it's easiest to skip, and the one that matters most. Before adding a second creature, a second opponent, or any real content:

Play the Phase 2 loop repeatedly, yourself, across multiple sessions — not once to confirm it technically works, but enough times to notice what's actually tedious, confusing, or unsatisfying.

Get at least one other person to play it with zero explanation beforehand, and watch where they get confused or stuck. Problems that are invisible to the person who built the system are often obvious to a first-time player.

Tune numbers based on what that actually reveals — damage values, bonding difficulty, how long a fight takes — rather than guessing what "feels right" from just reading the design on paper.

Only after this feels genuinely fun, not just functional, move on to Phase 4. If it doesn't feel fun yet, that's the actual problem to solve next — not "we need more content," but "why doesn't the thing that already exists feel good."

This is the phase where most solo projects quietly fail — not from lack of features, but from adding more and more content on top of a core loop that was never actually confirmed to be fun in the first place.

## Phase 4 — Expand outward, staged

**Only once Phase 3 confirms the core loop is genuinely fun:**

Grow the roster: a handful of creatures (a starter trio plus a few wild ones), each with real per-species personality in how they behave before becoming bondable, not just reskinned copies of the first one.

Grow the world: one real, small explorable region — not a whole world, one town and its surrounding area — built around what's already proven fun.

Add the surrounding systems that support the loop rather than replace it: saving progress, a simple economy, a basic collection/registry screen, one boss-style encounter.

Only after that first real region is complete and playable start to finish does it make sense to plan the next region, and the one after that — building outward one proven, complete piece at a time rather than laying out every system for the entire game up front.

The throughline: at every phase, the question is "what's the smallest real, playable version of the next piece," never "what's the complete version of every system."

## Ongoing discipline rules (apply at every phase)

Verify every change actually compiles and runs before moving to the next one. Writing five features and then discovering which one broke the build is far slower than confirming each one works before starting the next.

Keep game logic separate from presentation wherever possible. Battle math, bonding rules, and party state should be checkable on their own, without needing animations, art, or UI to exist yet. This is what made it possible to catch a couple of real bugs immediately and precisely, instead of guessing where a subtle problem was hiding inside a fully-presented scene.

Commit working versions often, not just at the end of a big feature. A string of small, working commits is far easier to recover from than one giant change that half-works.

Say no to scope, on purpose, early. The full vision can and should be fully written down (that's what a design bible is for) without being fully built before anything is proven fun. Writing everything down once, then building a small fraction of it first, is very different from trying to build everything at once.

Treat "is this actually fun yet" as a real checkpoint, not a vibe. Phase 3 above isn't a one-time formality — it's worth returning to that same honest question every time real scope gets added, not just once near the start.
