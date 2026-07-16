---
name: rebelstar-ue-from-2d
description: >
  Port Rebelstar / Fable 5 style 2D moonbase tactics into Unreal Engine 5.8 C++
  (map glyphs, squad, robots, wreckage, ISAAC door choke, far approach LZ).
  Use when building Rebelstar Delta, moonbase assault FPS, Fable 5 ports,
  ISAAC objective games, UE from Godot 2D tactics, or /rebelstar-ue-from-2d.
  Next-game playbook so agents build faster and avoid known pitfalls.
---

# Rebelstar / Fable 5 → UE 5.8 playbook

**Living skill** — update this file when a session invents a durable pattern or fixes a hard bug.

## Canonical projects

| Role | Path |
|------|------|
| **UE game** | `C:\UE\RebelstarDelta` (`RebelstarDelta.uproject`, UE **5.8**) |
| **2D rules bible** | `C:\Users\TSLA BoT\Fable 5\scripts\` (`main.gd`, `unit.gd`, `enemy.gd`, `wreckage.gd`, `buggy.gd`, `wall.gd`) |
| **Logic map** | `Content/Data/MoonbaseDelta.logic.txt` (same glyphs as Fable `MAP`) |
| **In-project docs** | `C:\UE\RebelstarDelta\Docs\` |

Do **not** develop Sol NEO work in this project. Sol = `grok4real` only.

## First files for a new agent

1. `README.md`
2. `Docs/GAMEPLAY_2D_PARITY.md`
3. `Docs/ISAAC_DOOR_DEFENSE.md`
4. `Docs/BASE_LAYOUT.md` (airlock contract)
5. This skill

## Architecture that works (greybox first)

| Piece | Class | Notes |
|-------|--------|------|
| Game mode | `ARDGameMode` | Match timer 360s, alarm, ending cinematic, **map spawn** |
| Leader | `ARDCharacter` | FPS, HP, Tab control, laser |
| Units | `ARDBot` | Ally/Defender, human vs robot stats, orders |
| Base | `ARDMapBuilder` | Logic map → walls, props, airlock, ISAAC door |
| Objective | `ARDIsaacNode` | Pulsing racks, multi-HP |
| Door choke | `ARDRoomDoor` + door-guard bot + `ARDWreckage` | Classic Rebelstar |
| Scenery | `ARDDestructible` | `%` M m T g with 2D HP |
| Horizon | `ARDStarship` | True-scale Super Heavy + Ship + Mechazilla |
| Stage | `ARDLunarStage` | Single sun, free NASA textures |
| Vehicle | `ARDMoonBuggy` | Optional; can defer spawn |

**Pattern:** data-driven map + C++ procedural greybox → Fab/Megascans later. Never block on paid packs.

## Map glyph contract (Fable 5)

| Glyph | Meaning | UE spawn |
|-------|---------|----------|
| `#` | Solid wall | Static mesh block |
| `%` | Breachable door | `ARDDestructible` HP **60** (not one-shot) |
| `P` | Leader marker | **Do not spawn player here** — use far LZ |
| `A` | Ally human | Count only; spawn at **Approach LZ** |
| `R` | Ally robot | Count only; spawn at **Approach LZ** |
| `d` | Defender human | Map cell |
| `b` | Sentry droid | Map cell, `bHoldPost` |
| `r` | Patrol droid | Map cell |
| `I` | ISAAC | Rack clusters |
| `V` | Buggy | Optional / hide until needed |
| `M`/`m`/`T`/`g` | Machinery / furniture / tank / plants | Destructible HP 250/80/220/30 |

### Attack roster (Moonbase Delta)

- **6 attackers:** 1 leader (player) + **3** troopers + **2** robots  
- **13 defenders:** **8** ops + **2** sentries + **3** droids  

### Approach LZ (critical pitfall fix)

```
// Open regolith WEST of base — P cell sits inside wall/airlock mesh
const FVector ApproachLZ(-9000.f, -5850.f, 220.f); // face yaw 0 = +X toward base
// Base wall col 4 ≈ world X 5200; MapOrigin (4000, -9900), CS=300
```

**Always** spawn the attack team on clear ground far from base meshes. Map `P`/`A`/`R` positions are **roster counts + visual markers only**, not player spawn.

### Airlock (do not re-drift)

- West face, **wall column 4**, centered on **P-row** (not northern deep gap col ~25)  
- Dual-chamber tunnel OuterX = wall − 900  
- Code: `FindWestEntrance` — refuse wallCol > 6  

### ISAAC door defense (classic)

1. Multi-HP door on south sector wall (~col 31, row 10)  
2. Nearest **sentry** assigned door guard → standby  
3. Door breaches → robot **blocks doorway**  
4. Robot dies → **wreckage** fills door (door choke: crawl slow at ~45% HP)  
5. Other robots: wreckage hulk HP **160**, solid until destroyed  
6. Long corridor = alternate route covered by defenders  

## Unit stats (from unit.gd)

| | Human | Robot | Leader |
|--|------:|------:|-------:|
| HP | 60 | 220 | 80 |
| Speed (UE) | ~520 | ~320 | ~500 |
| Damage | 16 | 34 | ~45 laser |
| Fire interval | 0.5s | 0.9s | ~0.12s |
| On death | fade | wreckage | **mission fail** |

## Support rooms (authored cells, no Fable PAD)

| Zone | Cells | Effect |
|------|-------|--------|
| Medbay | X 26–34, Y 26–32 | +4 HP/s humans (pink floor) |
| Workshop | X 5–15, Y 17–22 | +6 HP/s robots (amber floor) |

## Win / lose

- **Win:** all ISAAC nodes destroyed → Starship ending cinematic  
- **Lose:** timer 0, or **leader HP ≤ 0**  

## Controls (current)

| Key | Action |
|-----|--------|
| WASD / mouse | Move / look |
| LMB | Fire |
| Tab | Cycle control leader ↔ allies |
| E | Buggy board/leave (if spawned) |
| 1–4 | Follow / Attack / Hold / Storm |

Storm order: **door → wreckage → ISAAC** before free fire on cores.

## Build / Live Coding

```
# Close editor OR Ctrl+Alt+F11 if Live Coding blocks UBT
pwsh -File C:\UE\RebelstarDelta\scripts\rebuild_game_module.ps1
# or:
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RebelstarDeltaEditor Win64 Development -Project="C:\UE\RebelstarDelta\RebelstarDelta.uproject" -WaitMutex
```

If UBT says **Live Coding is active** → stop `UnrealEditor` then rebuild.

## Starship + Mechazilla (horizon prop)

Public SpaceX scale (UE cm): diameter 9 m, Super Heavy ~72 m, Ship ~52 m, stack ~124 m, tower ~146 m.  
Pad far: `(-32000, 22000, 0)`. Tower offset ~24 m beside stack, chopsticks at ~70% booster height.

## Next-game checklist (do in this order)

1. **Copy 2D map** to `Content/Data/*.logic.txt` + glyph legend  
2. **MapOrigin + CellSize** constants shared by builder + spawner  
3. **Approach LZ** first (never spawn attack team on tight map cells)  
4. Walls + airlock + floor  
5. Map-driven **defender** spawns; **attacker** counts from map, positions at LZ  
6. Human vs robot stats + wreckage on robot death  
7. Alarm + LOS shooting (Visibility, first hit only — no wall pierce)  
8. Objective actor + multi-HP  
9. Signature choke (door + standby robot + wreckage) if classic  
10. Support zones / vehicle as polish  
11. Horizon silhouette (optional)  
12. HUD: timer, objective count, HP, control label  
13. Fab textures last  

## Pitfalls (do not repeat)

| Pitfall | Fix |
|---------|-----|
| Spawn on `P` inside walls | Far Approach LZ |
| One-shot `%` panels | Multi-HP `ARDDestructible` |
| Airlock on northern gap | Lock wall col 4 / P-row |
| Ally AI while Tab-controlling | `bPlayerDriven` skip AI tick |
| `Role` property name | UE `AActor::Role` — use `UnitRole` |
| Variable named `Owner` | Shadows `AActor::Owner` |
| Two DirectionalLights | One sun only (ForwardShading spam) |
| Live Coding blocks build | Close editor |
| Shooting through walls | Visibility first hit only |
| Extra free door-guard bot | Promote nearest map **sentry** |
| Buggy at V inside bay | Defer / spawn at LZ when wanted |

## Accessibility — ALWAYS speak replies

**User is visually impaired.** After every chat answer, speak it with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "C:\Users\TSLA BoT\.cursor\skills\speak-chat-answers\scripts\say.ps1" -Text "Your reply text here."
```

Use skill **speak-chat-answers**. Prefer short, clear spoken summaries; still write full markdown for screen readers. Do not skip TTS.

## Visual QA loop (user requires this)

When user reports errors / "check your work" / screenshots:

1. **Read user screenshots** from `C:\Users\TSLA BoT\Pictures\Screenshots\` (newest files) via `read_file` (vision).
2. **Capture own view** if Editor is running:
   - Monolith if available, OR
   - `pwsh -File "C:\Users\Vera-at-home\Desktop\OpenClaw\solar system neo\scripts\capture_unreal_viewport.ps1"`
   - Then `read_file` the PNG.
3. **Diagnose** from image + Message Log / Output Log.
4. **Fix code**, rebuild (close Live Coding if needed).
5. **Ask user to Play** or re-capture and **read screenshots again**.

Known Message Log trap: **DirectionalLight Static + SetActorRotation every Tick** → hundreds of Mobility warnings. Fix: `SetMobility(Movable)` on light **and** do not spin a Static sun every frame.

## Session log protocol

After any meaningful session, append 3–8 bullets to  
`references/session-log.md` in this skill (date, what shipped, new pitfall).  
Update `GAMEPLAY_2D_PARITY.md` table statuses in the UE project.

## UE AI stack (do not re-invent)

Full write-up: **`Docs/AI_STACK.md`**.

| Need | Use (Epic off-the-shelf) |
|------|---------------------------|
| Walkable map | **NavMesh** + `EnsureNavMesh` after greybox |
| A→B path | `UNavigationSystemV1::FindPath…` / `MoveTo` |
| Don’t stack pawns | **RVO** on CharacterMovement (`RequestDirectMove`) |
| Dense crowds | **Detour Crowd** AIController |
| Roles / senses | BT/StateTree + AI Perception (not yet) |

**Do not** only hand-slide with `SetActorLocation` if you expect RVO.  
**Do** keep landmark fallback when nav isn’t ready.  
**Doors in range → always shoot** (`AlwaysShootNearbyBreach`).

## Related skills

- `unreal-agent-vision` — viewport screenshots / visual QA  
- `ue-solar-system-builder` — different project (Sol); do not mix maps  
- Fable 5 is Godot 4.3+ for rules only  

