# Playing the ClassiCube Survival prototype

This document describes the current source branch. It is not a claim that a tested Windows executable is available yet.

## Starting a world

1. Launch a build of this fork (once the Windows build has passed).
2. Choose Singleplayer and create or load a world.
3. Use the normal non-Classic game mode. Survival does not run in Classic mode or multiplayer.
4. When the world loads, the survival meters appear and a few passive animals spawn nearby.

## Controls

- WASD — move.
- Space — jump.
- Left mouse button (hold) — mine the selected block; the progress bar fills while held. Block drops appear in the world.
- Walk close to dropped blocks and mob loot to collect them.
- Left mouse button — attack a nearby mob.
- Right mouse button — place the selected block or eat the selected food.
- T — open chat to enter commands.
- B — open the inventory picker (default ClassiCube inventory binding).
- 1–9 — select a hotbar slot.
- Use the existing Controls menu to change any binding.

## Crafting

Open chat and enter /craft help to see the recipes.

Common starting progression:

1. Mine tree logs.
2. Enter /craft planks to make planks.
3. Enter /craft sticks to make sticks.
4. Enter /craft table, then place the crafting table.
5. Mine stone with a pickaxe and make a furnace with /craft furnace.
6. Use /craft wooden_pickaxe, /craft stone_pickaxe, and later /craft iron_pickaxe near a crafting table.
7. Mine coal, iron ore and gold ore with the required pickaxe tier.
8. Use /craft smelt_iron or /craft smelt_gold near a furnace. These commands consume coal immediately.
9. Use /craft cook_pork, /craft cook_beef or /craft cook_chicken near a furnace to cook meat.
10. Use /craft wooden_sword, /craft stone_sword or /craft iron_sword near a crafting table to improve melee damage.

Food is used by selecting it in the hotbar and right-clicking. The inventory picker and hotbar show stack counts. Dropped items remain in the world when the map is saved and reloaded.

## Saving

Use ClassiCube's normal pause/save-level flow to save a map. The fork writes a companion .survival file beside the map to preserve player survival data, inventory, tools and day/night time. Keep that companion file with the map when moving it to another computer.

## Known differences

This is an early Survival implementation, not a complete Beta 1.7.3 port. It still lacks infinite Beta terrain and biomes, a full Minecraft-style dropped-item despawn/merge system, a crafting-grid GUI, a timed furnace UI, armor, breeding, several mobs and other Beta features. The Windows build and gameplay still need verification.
