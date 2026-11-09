# ClassiCube Survival fork — current implementation

This branch adds a lightweight Survival mode to ClassiCube. It runs in normal singleplayer worlds only; Classic mode and multiplayer retain their original behaviour.

## Implemented

### Player survival
- 20 health points, 20 hunger points and saturation/exhaustion.
- Pixel-drawn heart and hunger icons, plus air bubbles while underwater.
- Underwater air reserve and drowning damage.
- Fall damage, with water cancelling fall damage.
- Lava damage, hunger depletion, exhaustion from movement/jumping, starvation and well-fed health regeneration.
- Death delay and respawn at the world spawn.
- Iron helmet, chestplate, leggings and boots can be equipped by right-clicking them from the hotbar. The armor HUD shows protection; armor reduces hostile-mob damage and loses durability as it protects the player.
- Equipped armor is dropped on death with its remaining durability and can be collected again.
- The player's inventory drops at the death location. The player cannot collect those drops until respawn.

### Inventory, items and blocks
- 36 inventory stack slots, with normal items stacking to 64 and tools stacking to 1.
- Survival inventory integrated with ClassiCube's inventory picker and hotbar; hotbar item counts are displayed.
- Food can be eaten with right-click even when the crosshair is not on a block.
- Holding the break input builds mining progress; releasing it or changing targets cancels progress.
- Basic harvest-tier rules and drops for common blocks and ores.
- Broken blocks, mob loot and death inventory create visible dropped-item entities that can be collected by walking close.
- A fixed 40-entity dropped-item pool; nearby same-item stacks merge, drops despawn after five minutes, and lava destroys them.
- Tool durability is retained when a tool is dropped and collected.
- Block placement consumes an item from the inventory.
- Local Survival items include food, seeds, wheat, sticks, coal, ingots, crafting table, furnace, torches, hoes, pickaxes, axes, shovels, swords, four iron armor pieces and mob drops.

### Farming
- Dandelion flowers can drop seeds.
- Wooden, stone and iron hoes can be crafted and used on dirt/grass to make farmland.
- Seeds can be planted on top of farmland.
- Crops need an open sky view; water within four blocks hydrates nearby farmland.
- Hydrated crops advance one growth stage every 90 in-game seconds; dry crops take 180 seconds per stage.
- Mature wheat drops wheat and additional seeds; three wheat can be crafted into bread.
- Crop tracking is capped at 512 plants. Crop stages are saved as map blocks; the partial growth timer resets when a map is reloaded.

### Crafting and smelting
- Chat command: `/client craft help`.
- Recipes include planks, sticks, crafting table, furnace, torches, wooden/stone/iron tools and swords, hoes, bread, ingots and cooked meat.
- Crafting recipes check for a nearby crafting table or furnace where required.
- Smelting/cooking recipes now run as timed jobs: each item takes 10 seconds and one coal provides 80 seconds of fuel. Up to 16 jobs of the same output type can be queued. Fuel and progress survive save/load. There is still no graphical furnace interface.

### World and mobs
- The existing ClassiCube Notchy generator creates a finite noise-based world with hills, caves, ores, water, flowers and trees. Normal Survival worlds use 128 × 128 horizontal dimensions and 128-block height on standard builds.
- A 20-minute day/night cycle adjusts sky, fog and sunlight colours.
- Passive pigs, chickens and sheep spawn when a world loads and wander.
- Zombies, skeletons, creepers and spiders can spawn at night and pursue the player. Zombies, skeletons and spiders attack at close range. Creepers pause and hiss with a 1.5-second fuse, then damage nearby players and break nearby non-bedrock blocks.
- Zombies and skeletons burn in direct daylight and are safe under a roof.
- Passive mobs drop meat or wool; hostile mobs drop rotten flesh, bones, arrows, gunpowder or string.

### Saving
- The map stores placed blocks and crop growth stages in the normal ClassiCube map data.
- A companion `.survival` sidecar stores health, hunger, air, inventory stacks, tool durability, hotbar layout, day/night time, dropped item stacks with positions/durability, and the furnace queue/fuel/progress.
- Sidecar version 7 stores equipped armor and reads versions 1–6 as well.
- Keep the `.survival` sidecar beside its map when moving the world.

## Still missing compared with Minecraft Beta 1.7.3

- Infinite terrain and Beta biomes; worlds are finite ClassiCube maps.
- A real crafting-grid screen and a timed furnace with fuel, progress and output slots; crafting/smelting are currently chat commands.
- Gold/leather/chain armor, armor rendered directly on the player model, bows/projectile combat, creeper explosions, cows, breeding, and several other original mob systems.
- Exact Beta block/item textures and models for all added items; farmland/crops currently reuse existing Classic textures.
- Full item physics such as water floating and a complete stack-merging system. The current drop pool is intentionally bounded for old PCs.
- A full play-test on the target Windows 7 / Intel GMA 950 machine. CI compilation is not the same as testing the game in that environment.

## Build status

The 32-bit Windows Direct3D 9 and OpenGL 1.1 clients compiled successfully in GitHub Actions for source commit `84f2917c9458d992cf6401ab425ec62f97d0e4ac`. The executables are in the `ClassiCube-Survival-Windows-32-bit` Actions artifact. The client still needs in-game testing on the target PC.
