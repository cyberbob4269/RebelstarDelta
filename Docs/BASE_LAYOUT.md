# Base layout (from Rebelstar 1986 / Fable 5)

Logic source: `Content/Data/MoonbaseDelta.logic.txt` (from Fable 5 / classic Rebelstar map).

| Zone | Notes |
|------|--------|
| **West airlock (LOCKED)** | **P-row approach, wall column 4** — dual-chamber tunnel on west face. Do **not** place on the northern deep gap (rows with first wall near col ~25). |
| Reactor | NW |
| ISAAC core | Top-centre — win objective |
| Crew / dorm | NE / SE |
| Hydroponics | East |
| Offices / droid bay / stores | West mechanical |
| Plaza + tanks | Centre |
| Labs / medbay | South |
| Vehicle bay | SW |
| Construction wing | Live dig / block stacks (mostly-built finish) |

## Airlock contract

```
MapOrigin = (4000, -9900), CellSize = 300
P marker ≈ row 13, col 2  →  world ~(4750, -5850)
West wall face = col 4     →  world X = 4000 + 4*300 = 5200
Airlock: OuterX = wall face − 900, InnerX = wall face
Doorway carve: wall cols 4–5, rows EntranceY±1 (centered on P)
```

Code: `ARDMapBuilder::FindWestEntrance` — prefers **P-row**, refuses wallCol > 6.

Scale: inflate 2D tiles into multi-meter corridors (Phase 1+).
