# Survival Mode fork — implementation status

This repository is a separate ClassiCube fork intended to grow into a lightweight, singleplayer Minecraft Beta 1.7.3-style Survival Edition.

## Current milestone: survival player-state foundation

- 10 hearts (20 health points) and a health HUD meter.
- 10 food points (20 food points) and a hunger HUD meter.
- A 15-second underwater air reserve, air HUD meter, and drowning damage.
- Fall-distance damage after falls longer than three blocks.
- Lava-contact damage.
- Exhaustion from walking and jumping, food/saturation depletion, natural regeneration, and starvation.
- A delayed death/respawn flow using the existing local-player spawn system.
- Public APIs for future food, healing, and damage sources.

Survival is active only in the internal singleplayer connection. Existing Classic multiplayer does not receive local health, hunger, or damage behavior.

## Not yet complete

1. Stackable item inventory, item entities, pickup and drop.
2. Survival mining progress, tool requirements, block drops, and tool durability.
3. Crafting grid, recipes, crafting table, furnace, smelting, and fuel.
4. Survival-specific Beta-style world generation, including ores, caves, trees, water and lava distribution.
5. Hostile and passive mob AI, spawning, combat, drops, and animal breeding.
6. Armor, equipment, damage mitigation, and remaining player damage sources.
7. Persistent singleplayer world/player saves and save migration.
8. Survival menus, difficulty settings, and full HUD artwork.

The current milestone is isolated in src/Survival.c and src/Survival.h so later systems can call its APIs without mixing survival logic into the Classic network protocol.

## Build / test note

The build gathers all C files in src automatically. This change has not yet been compiled or play-tested in this environment. Run the Windows build and test singleplayer before treating the milestone as release-ready.
