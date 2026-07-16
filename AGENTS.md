# Rebelstar Delta — agent brief

**Project:** `C:\UE\RebelstarDelta`  
**Engine:** Unreal Engine **5.8**  
**2D rules source:** `C:\Users\TSLA BoT\Fable 5` (do not develop the Godot game unless asked)

## First files

| Order | File | Why |
|-------|------|-----|
| 1 | `README.md` | Open / play |
| 2 | `Docs/GAMEPLAY_2D_PARITY.md` | What is ported from 2D |
| 3 | `Docs/ISAAC_DOOR_DEFENSE.md` | Door choke flow |
| 4 | `Docs/BASE_LAYOUT.md` | Airlock contract |
| 5 | Skill **`rebelstar-ue-from-2d`** | Next-game playbook + pitfalls |

## Critical rules

1. **Attack spawn = Approach LZ** `(-9000, -5850, 220)` facing +X — **never** map `P` cell (stuck in mesh).
2. **Airlock** = west face wall **col 4**, **P-row** — not northern deep gap.
3. **Roster from map glyphs**; defender positions on map; attackers counted from map, placed at LZ.
4. **Robots leave wreckage.** Door sentry blocks after ISAAC door breach.
5. Close Editor if Live Coding blocks UBT rebuild.

## Rebuild

```powershell
# Editor closed preferred
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" RebelstarDeltaEditor Win64 Development -Project="C:\UE\RebelstarDelta\RebelstarDelta.uproject" -WaitMutex
```

## Product intent

Cinematic **moonbase assault**: far approach → airlock → ISAAC door fight → destroy ISAAC → Starship ending. Greybox first; Fab Moon packs optional later.

## Accessibility

**User is visually impaired — always speak chat replies** via skill `speak-chat-answers`
(`say.ps1` with the reply text). Never skip TTS.

## Monolith MCP (enable for agent eyes + tools)

Rebelstar Delta now has a **junction** to the Monolith plugin  
(`Plugins/Monolith` → grok4real’s Monolith). Enabled in `RebelstarDelta.uproject`.

| Check | Value |
|-------|--------|
| Endpoint | `http://127.0.0.1:9316/mcp` |
| When | **Only while Rebelstar Delta editor is open** with plugin loaded |
| Log | Output Log: Monolith listening on **9316** |

If connection refused: open **this** project (not only grok4real), wait for plugin init,  
or run `monolith_proxy` if your MCP client uses it. Monolith gives BP/material/editor  
actions + discover — **use it** for visual QA when live.

## Earth textures

Raw files: `Content/RawTextures/earth_8k_*.jpg`  
On editor start, `Content/Python/init_unreal.py` **force-refreshes** 8k day/night/clouds  
into `/Game/RD/Textures` + materials. Pale Earth = stale 2k day or missing reimport.

## Skill hygiene

After meaningful sessions: append `session-log.md` in skill `rebelstar-ue-from-2d` and update parity docs.
