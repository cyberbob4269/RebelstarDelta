# Future-self notes — Rebelstar Delta (2026-07-16)

**Project:** `C:\UE\RebelstarDelta` · UE **5.8** · GitHub `cyberbob4269/RebelstarDelta`  
**Mode:** AI-vs-AI continuous training (default). Spectator follows Trump → succession.

---

## What shipped this session (must not regress)

1. **Pathing** — sticky multi-waypoint paths (NavMesh `FindPath` when available, landmark fallback).  
2. **RVO** — CharacterMovement avoidance + swept steps (don’t rely on CMC alone).  
3. **Formation** — unique slots 0–7 wedge (±7–14 m flanks).  
4. **Anti-snag** — décor/bricks `RD_Clutter` ignore pawn; doors stay solid.  
5. **Smooth cam** — no orbit bob; lagged subject; pitch clamp.  
6. **Objectives** — `TickAllyObjectives`: doors → sealed panels → **ISAAC** → enemy.  
7. **Door fire** — always shoot ISAAC door / wreck within ~14–16 m.  
8. **Speed** — humans ~760–820, robots ~560 (was crawl after flying-only RVO).  
9. **Data** — CSV + JSON after every round (see below).

---

## Data collection pipeline

| Output | Path |
|--------|------|
| Live series CSV | `Saved/AgentVision/training_series.csv` |
| Per-round text | `Saved/AgentVision/battle_report_R{n}.txt` |
| Latest JSON | `Saved/AgentVision/latest_round.json` |
| **Durable copy** (git-friendly) | `Content/Data/Training/training_series.csv` + `latest_round.json` |

CSV columns (v2):  
`ts,round,winner,usRoute,ukPlan,aliveA,aliveD,kA,kD,door,isaac,peakX,avgX,stuck,spd,dmg,fire,defFire,defSpd,winsUS,winsUK`

**Continuous soak:** `TargetTrainingRounds = 0` (unlimited rematch). Short matches ~70 s.  
**Export snapshot:** `pwsh -File scripts/export_training_snapshot.ps1`

---

## First files for next agent

1. `README.md`  
2. `Docs/FUTURE_SELF.md` (this file)  
3. `Docs/AI_STACK.md` — Epic NavMesh / RVO / Detour Crowd vs what we use  
4. `Content/Data/AGENT_NOTES.md`  
5. Skills: `rebelstar-ue-from-2d`, `rebelstar-agent-combat`  
6. `Content/Data/Training/training_series.csv` — latest soak results  

---

## Known next upgrades (do not re-invent)

| Priority | Item |
|----------|------|
| P1 | Thin `AIController` + real `MoveTo` when nav is solid |
| P1 | Nav modifier: closed ISAAC door = blocked area |
| P2 | Detour Crowd if corridor crush returns |
| P2 | AI Perception for alarm (replace pure distance) |
| P2 | Persist learn multipliers to disk across editor restarts |

---

## Build / open

```powershell
pwsh -File C:\UE\RebelstarDelta\scripts\rebuild_game_module.ps1
pwsh -File C:\UE\RebelstarDelta\scripts\open_editor.ps1
# Play = continuous AI-vs-AI; data auto-appends
```

Live Coding: close editor if UBT says Live Coding active.

---

## Pitfalls (session)

- `RequestDirectMove` alone (flying, no AIController) → **too slow**; keep swept `SetActorLocation` + feed RVO.  
- `FormationSlot = UniqueID % 3` → huddle; use spawn `SetFormationSlot`.  
- Brick yard BlockAll → snags; use `bBlockPawns=false` for décor.  
- Camera sine orbit → motion sickness; fixed shoulder only.  
- Door LOS reject on greybox frames → skip LOS for close doors.  

---

## Product intent (still true)

Moonbase assault FPS / spectator training for LSST-adjacent demo energy: big lasers, Trump follow-cam, US vs UK, ISAAC win. Not Sol NEO (that’s `grok4real`).
