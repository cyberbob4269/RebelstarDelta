# Real-world inspiration (mood + accuracy notes)

## Moon base (mood)

- NASA Artemis Base Camp — modular habitats, power, mobility
- Foundation Surface Habitat — airlock-first stacks (dual hatch)
- ESA crater / regolith-shielded inflatables
- Lunar ISRU: robots mining regolith → bricks / printed structures
- maps.speccy.cz Rebelstar 1 map (classic layout)

## Starship + Mechazilla (scale targets)

Public SpaceX / Starbase figures used in `RDStarship.cpp` (UE cm):

| Part | Height | Notes |
|------|--------|--------|
| Diameter | **9 m** | constant |
| Super Heavy | **~72 m** | booster |
| Ship | **~52 m** | upper stage |
| Full stack | **~124 m** | stacked on OLM |
| Mechazilla tower | **~146 m** | chopsticks + QD arms |
| OLM visual | **~18 m** | pad plinth under stack |

Layout: stainless stack on pad, **Mechazilla immediately beside** (~24 m offset), chopsticks at ~70% Super Heavy height, yellow catch pads, QD boom mid-booster, pad apron + flame trench + perimeter floods.

Horizon pad world: `(-32000, 22000, 0)` — distant silhouette from base approach / ending camera.
