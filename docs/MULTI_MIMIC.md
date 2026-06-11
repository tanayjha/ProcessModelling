# Integrated multi-mimic plant simulation

This note describes how UMPNAP lets a plant be drawn as **several mimic files**
(one per subsystem) that are **reused and simulated together as one integrated
network**, so whole-plant dynamics are visible when all subsystems run in unison.

## Motivation

A real plant is too large for one diagram. Engineers draw it as separate mimics
— a compressor house, an instrument-air distribution network, a cooling-water
loop, an electrical single-line — each in its own file. But those subsystems are
physically coupled: the compressor house and every instrument-air consumer share
the **same** air receiver. To see plant behaviour you must solve them **together**,
not one file at a time.

Two requirements follow:

1. **Reuse one piece of equipment across files.** The plant air receiver is
   defined once and referenced by every mimic that taps it — not copy-pasted.
2. **Run all mimics of a project in unison** and observe the integrated dynamics
   (e.g. receiver pressure responding to compressor supply minus the summed
   instrument-air demand across every subsystem).

## Concepts

### Mimic (`.umpnap`)
One subsystem network: components + connections, exactly as today. Unchanged.

### Plant project (`.umpproj`)
A small JSON manifest naming the member mimics:

```json
{
  "version": 1,
  "name": "Demo Air Plant",
  "mimics": ["compressor_house.umpnap", "instrument_air.umpnap"]
}
```

Mimic paths are resolved **relative to the manifest's own directory**.

### Link tag (`config["linkTag"]`)
A stable, plant-global identifier set on a component (in the Properties dock,
"Link tag" field, available for shareable equipment: AirReceiver, Header, Tank,
PressurizedTank, Boundary, Junction). Components in **different mimics** that
carry the **same non-empty link tag denote the same physical equipment**.

To reuse the plant air receiver, drop an `AirReceiver` in each mimic that needs
it and give them all the same link tag, e.g. `AR-PLANT-1`. One mimic fully
specifies its parameters (the *canonical* definition); the others are just
references.

## How the integrated run is built (the methodology)

Loading a `.umpproj` performs a **structural merge** of all member mimics into a
single `Network`, which the **existing nodal solver runs unchanged**:

1. Load every member mimic into its own `Network`.
2. Walk the mimics in manifest order. Copy each component into the merged network
   with a fresh id, **except** when a component's link tag has already been seen
   — then it is *not duplicated*; the earlier (canonical) instance survives and
   this reference is dropped.
3. Rewire every connection onto the surviving instances (a pipe that connected to
   a dropped receiver reference is reattached to the canonical receiver).
4. Non-shared components of each mimic are shifted into their own vertical band on
   the merged canvas so the integrated diagram stays readable; shared equipment
   keeps its canonical position.

The result is **one network with one shared receiver node** that every subsystem
connects to. Because the air receiver collapses all of its tappings (`p`, `n1…
n19`) onto a single vessel pressure node, the compressor's supply and every
consumer's demand meet at that one node — the whole plant balances in a single
solve. Steady and transient runs, trends, and CSV export all then cover the
entire plant at once.

This reuses the entire solver and file format; the only new engine primitive is
`mergeMimics()` (in `core/Project.{h,cpp}`).

```
PlantProject              loadPlantNetwork()                 SolverManager
 .umpproj  ──────►  load each mimic ──► mergeMimics()  ──►  one Network  ──► run
            (manifest)        (linkTag unify + rewire + band layout)   (unison)
```

## Using it

- **GUI:** `File ▸ Open Plant Project…`, pick the `.umpproj`. The merged plant
  loads onto the canvas; press Run to see whole-plant dynamics. Set a component's
  **Link tag** in the Properties dock to make it shareable.
- **CLI:** `umpnap path/to/plant.umpproj [--run]`.
- **Headless render:** `umpnap_screenshot plant.umpproj out.png`.
- **Worked example:** `make_plant` writes `examples/compressor_house.umpnap`,
  `examples/instrument_air.umpnap` and `examples/plant.umpproj`; both mimics share
  one receiver via link tag `AR-PLANT-1`, and the merged plant solves with the
  compressor charging the receiver while the instrument-air valve draws from it.

## Current limitations / extension points

- The merge identifies equipment by link tag; **the first occurrence's
  parameters win**. Keep one canonical definition per shared item.
- Component **tags should be plant-unique** across mimics — tag-based wiring (PID
  controller `measComp`/`output`) resolves to the first matching tag after merge.
- The air receiver is modelled as a **fixed-pressure plenum** (`p_top`); true
  compressible gas-inventory pressure dynamics await the dedicated gas solver.
- Per-mimic tabs/views and a project tree in the GUI are a natural follow-on; the
  engine merge and the manifest format are the foundation they would build on.
