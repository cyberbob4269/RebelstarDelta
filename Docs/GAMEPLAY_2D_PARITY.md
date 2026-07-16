# Gameplay parity — Fable 5 (2D) → Rebelstar Delta (UE)

Source: `C:\Users\TSLA BoT\Fable 5\scripts\`  
Map: `Content/Data/MoonbaseDelta.logic.txt`

## Roster (map-driven)

| Glyph | Role | Count |
|-------|------|------:|
| `P` | Leader (you) | 1 |
| `A` | Human troopers | 3 |
| `R` | Assault robots | 2 |
| `d` | Operatives | 8 |
| `b` | Sentry droids (hold post) | 2 |
| `r` | Patrol droids | 3 |
| `V` | Moon buggy | 1 |
| `I` | ISAAC | 2 racks |

**6 attackers** (you+3+2) vs **13 defenders**.

## Controls

| Key | Action |
|-----|--------|
| WASD / mouse | Move / look |
| LMB | Fire |
| **Tab** | Cycle control: you → allies → you |
| **E** | Board / leave moon buggy |
| 1–4 | Squad orders (Follow / Attack / Hold / Storm) |

## Unit stats

| | Human | Robot | Leader |
|--|------:|------:|-------:|
| HP | 60 | 220 | **80** |
| Speed | 520 | 320 | 500 |
| Damage | 16 | 34 | 45 (laser) |
| Death | fade | **wreckage** | **mission fail** |

## Mechanics

| Mechanic | Status |
|----------|--------|
| Map spawns P/A/R/d/b/r/V | Yes |
| Robot wreckage hulks | Yes (HP 160) |
| Sentry hold post | Yes |
| Alarm radio ~1.2s | Yes |
| ISAAC door + door guard | Yes |
| Door wreckage crawl slow | Yes |
| Humans kite robots | Yes |
| **Leader HP + death = lose** | **Yes** |
| **Destructible %/M/m/T/g** | **Yes** (2D HP) |
| **Moon buggy** | **Yes** (E) |
| **Medbay heal humans** | **Yes** (cells 26–34, 26–32) |
| **Workshop repair robots** | **Yes** (cells 5–15, 17–22) |
| **Tab control allies** | **Yes** |
| Retreat to heal AI | Partial (zones work; auto-retreat not yet) |
| Fog of war | No |
| Factions / barks | No |

## Destructible HP (wall.gd)

| Kind | HP |
|------|---:|
| Sealed door `%` | 60 |
| Machinery `M` | 250 |
| Furniture `m` | 80 |
| Hydroponics `g` | 30 |
| Water tank `T` | 220 |
| ISAAC room door | 220 |
| Robot wreckage | 160 |
| Door choke wreckage | 400 (crawl at 45%) |

## Support rooms (floor tint)

- **Medbay** — pink/red floor, heals humans +4 HP/s  
- **Workshop** — amber floor, repairs robots +6 HP/s  

## Win / lose

- **Win:** all ISAAC nodes destroyed  
- **Lose:** timer 360s, or **leader HP → 0**
