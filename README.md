# Rebelstar Delta

UE **5.8** C++ moonbase assault — Fable 5 / Rebelstar-style tactics in 3D.

**Default mode:** AI-vs-AI spectator loop (US assault vs UK defence), open roofs, tactical learning, close-run rosters.

## Teams (balanced ~6v7)

| Side | Roster |
|------|--------|
| **US** | Trump, Vance, Rubio, Haley + APEX-1, EAGLE-9 |
| **UK** | Starmer, Farage, Sunak, Truss + GCHQ-1 + Britannia, Lion |

Auto-balance nudges counts if one side wins a series by 2+.

## Features

- Humanoid **spacesuits** + US/UK chest **flag badges**
- **Tactical learning:** airlock / north flank / south flank · UK hold / rush / split
- **Humans never charge robots head-on** (kite; mechs tank)
- Wall-safe movement (sweep collision) + unclip
- Fat **laser beams** + procedural **laser zap** audio
- Training series logs: `Saved/AgentVision/training_series.csv`

## Open

```
C:\UE\RebelstarDelta\RebelstarDelta.uproject
```

Map / game mode is set in project defaults. Press **Play** for AI spectator orbit.

## Data

- Logic map: `Content/Data/MoonbaseDelta.logic.txt`
- Docs: `Docs/`

## Build

UE 5.8 + Live Coding (Ctrl+Alt+F11) or:

```bat
Build.bat RebelstarDeltaEditor Win64 Development -Project="...\RebelstarDelta.uproject"
```

## License / notes

Personal backup of an in-progress game. Engine content remains Epic's. NASA/SSS textures used for lunar stage where present under free terms.
