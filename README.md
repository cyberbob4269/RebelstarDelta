# Rebelstar Delta

UE **5.8** C++ moonbase assault — Fable 5 / Rebelstar-style tactics in 3D.

**Default mode:** AI-vs-AI **continuous** spectator training (US vs UK), open roofs, tactical learning, close-run rosters.  
`TargetTrainingRounds = 0` → unlimited rematch soak for data collection.

## Teams (balanced ~6v7)

| Side | Roster |
|------|--------|
| **US** | Trump, Vance, Rubio, Haley + APEX-1, EAGLE-9 |
| **UK** | Starmer, Farage, Sunak, Truss + GCHQ-1 + Britannia, Lion |

Auto-balance nudges counts if one side wins a series by 2+.

## Features

- Humanoid **spacesuits** + US/UK chest **flag badges**
- **Assault objectives:** doors → sealed panels → **ISAAC** → enemy
- **Tactical learning:** airlock / north flank / south flank · UK hold / rush / split
- **Humans never charge robots head-on** (kite; mechs tank)
- Nav path + landmark fallback, RVO, formation wedge, anti-snag clutter
- Smooth Trump follow-cam (succession) + fat lasers / zap SFX
- **Training pipeline:** CSV + JSON every round

## Open

```
C:\UE\RebelstarDelta\RebelstarDelta.uproject
# or: pwsh -File scripts\open_editor.ps1
```

Press **Play** — continuous AI spectator assault.

## Data / handoff

| Path | Why |
|------|-----|
| `Content/Data/Training/` | Durable series CSV + latest JSON |
| `Saved/AgentVision/` | Live logs + battle reports |
| `Docs/FUTURE_SELF.md` | Next-agent briefing |
| `Docs/AI_STACK.md` | UE NavMesh / RVO / what we use |
| `Content/Data/AGENT_NOTES.md` | Short agent card |

```powershell
pwsh -File scripts\export_training_snapshot.ps1
```

## Build

```powershell
pwsh -File scripts\rebuild_game_module.ps1
```

Live Coding blocks UBT → close editor first.

## License / notes

Personal backup of an in-progress game. Engine content remains Epic's. NASA/SSS textures used for lunar stage where present under free terms.
