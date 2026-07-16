---
name: rebelstar-agent-combat
description: >
  Drive Rebelstar Delta in UE PIE via Monolith: start Play (Alt+P / start_pie),
  HighResShot screenshots, teleport, aim, AgentFire laser, ApplyDamage on
  defenders, clear all red bots. Use when user wants agent to play, kill enemies,
  test combat, WASD/teleport patrol, or save agent control skills.
---

# Rebelstar Delta — agent combat control (Monolith)

**Project:** `C:\UE\RebelstarDelta` · Monolith `http://127.0.0.1:9316/mcp` · project_name **RebelstarDelta**

## Prerequisites

1. Unreal Editor open on **Rebelstar Delta** (not only grok4real).
2. Output Log: Monolith listening on **9316**.
3. Prefer **Play** already running, or call `editor.start_pie`.
4. Speak replies if user is visually impaired (`speak-chat-answers`).

## Start Play

```
editor.start_pie
```

If already running: `{"started":false,"reason":"PIE already running"}`.  
Alt+P toggles — avoid double-toggle. Confirm with Python:

```python
w = unreal.EditorLevelLibrary.get_game_world()  # non-None = PIE
pawn = unreal.GameplayStatics.get_player_pawn(w, 0)
```

## Screenshots (works)

```
editor.run_console_command
  command: HighResShot 1280x720 filename=C:/UE/RebelstarDelta/Saved/AgentVision/shot.png
```

Read PNG with `read_file` for vision QA.

## Movement

| Method | Works? | Notes |
|--------|--------|--------|
| `pawn.set_actor_location` | **Yes** | Primary |
| `pie_set_control_rotation` | **Yes** | Aim / look |
| `pawn.add_movement_input` | **Weak** | Often no motion in PIE agent tick |
| WASD key send to window | Fragile | Focus issues |

**Pattern:** teleport near target + set control rotation to face them.

## Shooting

| Method | Works? |
|--------|--------|
| `pawn.call_method('AgentFire')` | **Yes** (BlueprintCallable on RDCharacter) |
| `bot.call_method('ApplyDamage', (50.0,))` | **Yes** (reliable kill) |
| `OnFire` | No (not UFUNCTION) |

Kill sequence for one defender:

```python
# teleport 280cm in front, aim, AgentFire + ApplyDamage until IsDead()
```

## Clear all defenders (proven 2026-07-16)

1. `start_pie` if needed; wait ~3s for spawn.  
2. List bots: `Team` string contains `DEFENDER`.  
3. For each: drop if Z>180 → stand nearby → aim → `ApplyDamage` until `IsDead()`.  
4. HighResShot proof.  
5. Expect **13** defenders (8 ops + 2 sentries + 3 droids).

## Live Coding

```
editor.live_compile  # = Ctrl+Alt+F11
editor.get_build_status  # last_result success, patch_applied
```

## Anti-roof

Bots may still climb wall tops during AI. Code has anti-roof tick (Z>220 snap).  
Agent should still force `Z=112` if `Z>180` before combat.

## Files

- Shots: `C:\UE\RebelstarDelta\Saved\AgentVision\`
- Related skill: `rebelstar-ue-from-2d`
