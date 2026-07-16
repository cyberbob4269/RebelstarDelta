# Rebelstar Delta — UE AI stack (research + status)

**Date:** 2026-07-16  
**Engine:** Unreal Engine **5.8**  
**Scope:** What industry / Epic use for NPC movement, what we ship, what we still skip.

---

## The off-the-shelf stack (Epic docs)

Epic documents **two avoidance methods** on top of the **Navigation System** (Recast/Detour):

| Layer | What it is | Where in UE | Best for |
|-------|------------|-------------|----------|
| **NavMesh (Recast)** | Walkable surface mesh | `NavMeshBoundsVolume` + `UNavigationSystemV1` | Walls / corridors / doors as blocked cells |
| **Path query / MoveTo** | A* on nav mesh | `FindPathToLocationSynchronously`, `AAIController::MoveTo` | “How do I get from A→B?” |
| **RVO** | Reciprocal Velocity Obstacles | `UCharacterMovementComponent::bUseRVOAvoidance` | Agents not stacking into each other |
| **Detour Crowd** | Stronger adaptive RVO on mesh | `DetourCrowdAIController` / `UCrowdFollowingComponent` | Dense crowds (10+), stay on mesh |
| **AIController + BT/StateTree** | Decision brain | Behavior Tree, StateTree, EQS | Roles, cover, sensing |
| **AI Perception** | Sight/hearing | `UAIPerceptionComponent` | Alarm, LOS awareness |

Sources (Epic):

- [Navigation System](https://dev.epicgames.com/documentation/unreal-engine/navigation-system-in-unreal-engine)
- [Using Avoidance with Navigation](https://dev.epicgames.com/documentation/unreal-engine/using-avoidance-with-the-navigation-system-in-unreal-engine)

**Rule of thumb (industry):**

- **Few agents, simple levels:** Character RVO + simple goals.  
- **Many agents / corridors:** NavMesh + Detour Crowd.  
- **Tactics / roles:** BT or StateTree on top — **not** more hand-tuned `Tick` spaghetti.

RVO **does not replace** pathfinding: it only steers velocities so pawns don’t occupy the same point. Without NavMesh, agents still walk into **static walls** unless you sweep/detour yourself.

---

## What Rebelstar Delta has now

| Piece | Status | Notes |
|-------|--------|--------|
| Landmark + LOS path | ✅ | Greybox fallback always |
| Capsule walk LOS / detours | ✅ | Custom |
| Formation wedge / choke yield | ✅ | Custom tactics |
| Character **RVO** | ✅ | `bUseRVOAvoidance` + `RequestDirectMove` |
| **NavMesh bounds + Build()** | ✅ | `ARDMapBuilder::EnsureNavMesh` after greybox |
| Walls **CanEverAffectNavigation** | ✅ | Solid boxes only; clutter off |
| **FindPathToLocationSynchronously** | ✅ | Used first in `RebuildPathTo`; falls back to landmarks |
| Door always-shoot when close | ✅ | `AlwaysShootNearbyBreach` ≤14 m |
| Clutter pawn-ignore | ✅ | Bricks/poles not BlockAll for pawns |
| Smooth cam + damped move | ✅ | Spectator comfort |
| **AIController** | ❌ | `AutoPossessAI = Disabled`; Tick-driven |
| **Detour Crowd controller** | ❌ | Optional upgrade if 15+ agents jam |
| **Behavior Tree / StateTree** | ❌ | Roles are C++ `switch` |
| **EQS** (cover / flank query) | ❌ | Flanks are fixed formation slots |
| **AI Perception** | ❌ | Range + line traces only |
| **Nav Modifier** (closed door = blocked) | ❌ | Door is mesh block + shoot; not area cost |
| Full **MoveTo** path following component | ❌ | We consume path points in `MoveToward` |

---

## Gaps that still matter (priority)

### P0 — gameplay correctness (mostly done)
- [x] Don’t snag on décor  
- [x] Don’t huddle (formation + RVO)  
- [x] Shoot doors when close  
- [x] Prefer engine path when nav exists  

### P1 — next real upgrades
1. **Possess with a thin `ARDAIController`**  
   - Enables `MoveToActor` / `MoveToLocation` and future Detour Crowd.  
   - Keep tactics in C++ or migrate storm roles to BT later.

2. **Nav Modifier / dynamic obstacle on ISAAC door**  
   - While door HP > 0, mark area unwalkable or high cost so path routes to breach point, not through mesh.

3. **Detour Crowd** if training shows corridor crush with RVO only  
   - Swap controller path following to `UCrowdFollowingComponent`.

4. **AI Perception** for alarm  
   - Replace pure distance scans with sight sense (matches 2D “radio when spot”).

5. **EQS or simple cover scores** for humans kiting robots  
   - Query “nearest friendly robot” already exists; extend to “open LOS cell with distance band”.

### P2 — polish
- Mass Entity / MassAI — overkill for ≤20 units.  
- ML / learning agents — not needed for greybox tactics.  
- Full animation locomotion — no skeletal combat mesh yet.

---

## Agent rule (do not re-invent)

When improving movement:

1. **Prefer engine systems** (Nav path → RVO → Detour Crowd).  
2. Keep **landmark fallback** for when nav fails (PIE before Build, missing bounds).  
3. **Never** only `SetActorLocation` for crowd move if RVO is expected.  
4. Doors / objectives: **always shoot in range** — tactics secondary.

---

## Quick verify (PIE)

1. Play → Output Log should show `NavMeshBoundsVolume` + `NavigationSystem::Build()`.  
2. Allies near ISAAC door spam lasers on door mesh.  
3. Squad maintains lateral spacing on open regolith (RVO + formation).  
4. If `No NavigationSystem` warning → check `DefaultEngine.ini` nav section + module `NavigationSystem` in Build.cs (already linked).
