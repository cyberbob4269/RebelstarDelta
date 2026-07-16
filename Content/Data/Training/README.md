# Training data (durable)

Written every AI-vs-AI round by `ARDGameMode` (also under `Saved/AgentVision/`).

| File | Role |
|------|------|
| `training_series.csv` | Append-only round metrics |
| `latest_round.json` | Last finished round (overwrite) |
| `archive_*` | Snapshots from `scripts/export_training_snapshot.ps1` |

Analyze with Excel, Python pandas, or agent scripts. Continuous soak: Play in editor with `TargetTrainingRounds=0`.
