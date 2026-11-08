# Playing the ClassiCube Survival prototype

This guide describes the current source branch. The code still needs a successful Windows build and in-game verification before it can be called a finished playable release.

## Starting a world

1. Launch a build of this fork.
2. Choose Singleplayer and create or load a world.
3. Use the normal non-Classic game mode. Survival does not run in Classic mode or multiplayer.
4. When the world loads, the survival meters appear and a few passive animals spawn nearby.

## Controls

- **WASD** — move.
- **Space** — jump.
- **Left mouse button (hold)** — mine the selected block; the progress bar fills while held. Block drops appear in the world.
- **Walk close to drops** — collect block and mob items.
- **Left mouse button** — attack a nearby mob.
- **Right mouse button** — place the selected block or eat the selected food.
- **T** — open chat.
- **B** — open the inventory picker (default ClassiCube inventory binding).
- **1–9** — select a hotbar slot.

Use the existing Controls menu to change bindings.

## Crafting

Open chat and enter `/client craft help` to see the recipes.

Common starting progression:

1. Mine tree logs.
2. Enter `/client craft planks` to make planks.
3. Enter `/client craft sticks` to make sticks.
4. Enter `/client craft table`, then place the crafting table.
5. Mine stone and make a furnace with `/client craft furnace`.
6. Use `/client craft wooden_pickaxe`, `/client craft stone_pickaxe`, and later `/client craft iron_pickaxe` near a crafting table.
7. Mine coal, iron ore and gold ore with the required pickaxe tier.
8. Use `/client craft smelt_iron` or `/client craft smelt_gold` near a furnace. These commands consume coal immediately.
9. Use `/client craft cook_pork`, `/client craft cook_beef` or `/client craft cook_chicken` near a furnace to cook meat.
10. Use `/client craft wooden_sword`, `/client craft stone_sword` or `/client craft iron_sword` near a crafting table to improve melee damage.

Food is used by selecting it in the hotbar and right-clicking. The inventory picker and hotbar show stack counts.

## Saving

Use ClassiCube's normal pause/save-level flow to save a map. The fork writes a companion `.survival` file beside the map to preserve player state, inventory, tools, day/night time and dropped item stacks. Keep that companion file with the map when moving it to another computer.

## Known differences

This is an early Survival implementation, not a complete Beta 1.7.3 port. It still lacks infinite Beta terrain and biomes, a full crafting-grid interface, a timed furnace UI, armor, breeding, some animals, full item physics/merge behavior, and other Beta features. Dropped items are limited to 32 simultaneous stacks and despawn after five minutes. The Windows build and gameplay still need verification.
