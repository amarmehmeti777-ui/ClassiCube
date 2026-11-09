# Playing the ClassiCube Survival prototype

This guide describes the current Survival branch. Use a normal singleplayer world; Survival is disabled in Classic mode and multiplayer.

## Controls

- **WASD** — move.
- **Space** — jump.
- **Left mouse button (hold)** — mine the selected block; the progress bar fills while held.
- **Left mouse button** — attack a nearby mob.
- **Right mouse button** — place the selected block, eat selected food, till soil with a hoe, or plant seeds.
- **Walk close to drops** — collect block and mob items.
- **T** — open chat.
- **B** — open the inventory picker (default ClassiCube inventory binding).
- **1–9** — select a hotbar slot.

Food can be eaten with right-click even when you are looking at empty air.

## First Survival progression

1. Break a tree log and enter `/client craft planks`.
2. Enter `/client craft sticks`.
3. Enter `/client craft table`, then place your crafting table.
4. Craft a wooden pickaxe with `/client craft wooden_pickaxe` and mine stone.
5. Craft a stone pickaxe and then a furnace with `/client craft furnace`.
6. Mine coal and iron ore. Use `/client craft smelt_iron` near a furnace to get iron ingots.
7. Use `/client craft iron_pickaxe` near a crafting table for the iron pickaxe.

## Farming and bread

1. Break dandelion flowers to collect seeds.
2. Enter `/client craft wooden_hoe` (or craft a stone/iron hoe).
3. Hold the hoe and right-click a dirt or grass block with clear space above it to make farmland.
4. Place water within four blocks of the farmland to improve crop growth.
5. Select seeds and right-click the top of the farmland to plant them.
6. Crops grow in sunlight. Each stage takes about 90 in-game seconds near water or 180 seconds without nearby water.
7. Break mature wheat to collect wheat and more seeds.
8. Enter `/client craft bread` near a crafting table to turn three wheat into bread.

## Food, combat and death

Select food in your hotbar and right-click to eat. Pigs and chickens provide meat; sheep provide wool. Hostile mobs appear at night. Zombies and skeletons burn in direct daylight unless sheltered. If you die, your inventory drops at the death location; return after respawning to collect it. Lava destroys dropped items.

## Crafting and saving

Enter `/client craft help` in chat to see all recipes. Smelting/cooking commands now start timed furnace jobs: each item takes 10 seconds, one coal fuels up to eight jobs, and up to 16 jobs of the same output can be queued. The queue and remaining fuel are saved with the world. The furnace still has no graphical interface.

Use ClassiCube's normal pause/save-level flow. Keep the companion `.survival` file beside its map file when moving a world to another computer. Crop stages and placed farmland are saved in the map; the crop's partial growth timer restarts when the map is loaded.

## Known differences

This is still an early Survival port, not a complete Beta 1.7.3 recreation. It uses finite ClassiCube worlds, command-based crafting, an instant furnace recipe system, simplified mob AI, and placeholder Classic textures for some farming items. Armor, bows, creeper explosions, cows, breeding, infinite terrain and Beta biomes are not implemented yet. The Windows clients compile, but still need play-testing on the target PC.
