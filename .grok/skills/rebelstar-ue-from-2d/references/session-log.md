# Session log — rebelstar-ue-from-2d

## 2026-07-16 — Delta foundation + 2D port

- UE 5.8 project `RebelstarDelta`: lunar stage, map builder, ISAAC, squad, Starship+Mechazilla
- Airlock restored to P-row / wall col 4 (northern gap drift fixed)
- Classic ISAAC door: multi-HP door, sentry door-guard, wreckage choke + crawl slow
- Full Fable 5 roster from map glyphs; robot vs human stats; wreckage on all robot deaths
- Leader HP 80, death = lose; Tab control; medbay/workshop; destructible scenery
- **Pitfall:** spawning on map `P` stuck inside mesh → **Approach LZ (-9000, -5850, 220)**
- Attack team forms on open plain; buggy spawn deferred
- Docs: `GAMEPLAY_2D_PARITY.md`, `ISAAC_DOOR_DEFENSE.md`, `BASE_LAYOUT.md`
- Skill **`rebelstar-ue-from-2d`** written (Cursor + Grok + project mirrors) + `AGENTS.md`
- Approach LZ pad + chevron path to airlock; ally follow sprint scales with distance
- **Accessibility:** always speak replies (speak-chat-answers) — user visually impaired
- Earth pack: SSS/NASA 8k day + night cities + clouds; low sun long shadows; Earthrise spin day/night
- **Vision QA loop:** user screenshots + agent vision; Monolith when editor up; else Pictures\Screenshots + F9
- **Bugfix:** DirectionalLight Static + SetActorRotation every tick → 200+ PIE Mobility warnings → Movable + fixed sun
- **Bug:** LZ pad / spawn height trap — thinner pad, higher spawn, anti-stuck teleport
- **Bug:** enemies on roof — AlwaysSpawn + high Z ejected onto wall tops; floor-trace snap (reject Z>150), RehomeHere, ceilings ignore Pawn

## 2026-07-16 — Earth pale + Monolith
- Pale Earth: day texture was stale 2k (import skipped existing); force-reimport 8k + rebuild mats; remove wash halo; bigger closer Earth
- Monolith: RebelstarDelta had no plugin — junctioned from grok4real + enabled in uproject; listens 9316 only when THIS editor is open

## 2026-07-16 — pathfinding upgrade
- Bots no longer only wall-slide greedily: sticky multi-waypoint paths via `RebuildPathTo`
- Capsule `HasWalkableLOS` (ignores pawns), moonbase landmark graph (LZ → airlock → plaza → ISAAC)
- Side detours (`TryAppendDetour`) + flip prefer-side on stuck
- `MoveToward` follows path, shortcuts when LOS clears, repaths on fail/stuck
- Built DLL 2026-07-16 23:21 — full UBT after editor close

## 2026-07-16 — full AI session + continuous data backup
- Path, formation, anti-snag, smooth cam, RVO+NavMesh, objectives, speed fix
- Continuous training TargetTrainingRounds=0; CSV+JSON durable under Content/Data/Training
- Docs: FUTURE_SELF.md, AI_STACK.md, AGENT_NOTES.md; export_training_snapshot.ps1
- GitHub cyberbob4269/RebelstarDelta
