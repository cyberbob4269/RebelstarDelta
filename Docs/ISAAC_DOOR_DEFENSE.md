# ISAAC door defense (classic Rebelstar choke)

Attackers enter the **west airlock**, push toward ISAAC, and hit a **real door** — not a free one-shot breach panel.

## Flow

1. **Door closed** — multi-HP `ARDRoomDoor` on the south wall of the ISAAC sector (logic map ~col 31, row 10).
2. **Door destroyed** — alarm; the **standby defender robot** rushes into the doorway and holds.
3. **Robot destroyed** — **wreckage** fills the doorway (solid block).
4. **Wreckage damaged enough** (~55% chewed) — crawl-through opens but **heavily slows** movement.
5. **Wreckage cleared** — path open; now you can work ISAAC (high HP).
6. **Alternate route** — long east corridor is covered by extra defenders (“doesn’t end well”).

## Tunables

| Piece | Class | Default |
|-------|--------|---------|
| Door HP | `ARDRoomDoor::MaxHP` | 220 |
| Door guard HP | `AssignDoorGuard` | 140 |
| Wreckage HP | `ARDWreckage::MaxHP` | 400 |
| Passable at | `PassableAtFraction` | 0.45 |
| Crawl slow | `SlowMultiplier` | 0.28 |
| ISAAC HP | `ARDIsaacNode::MaxHP` | 900 |

## Code entry points

- Spawn door: `ARDMapBuilder::SpawnIsaacRoomDoor` / `FindIsaacDoorCell`
- Guard: `ARDBot::AssignDoorGuard` / `CommandBlockDoorway` / `TickDoorGuard`
- Events: `ARDGameMode::NotifyIsaacDoorBreached` / `NotifyDoorGuardKilled` / `SpawnDoorWreckage`
- Player slow: `ARDCharacter::Tick` wreckage check
- Squad storm: door/wreck before ISAAC (`ERDSquadOrder::Storm`)
