# ClassiCube Survival fork — current implementation

This branch adds a lightweight, local singleplayer Survival layer to ClassiCube. Survival is active in normal (non-Classic) singleplayer worlds only. Multiplayer and Classic mode keep their original behavior.

## Implemented in this branch

### Player survival
- 20 health points, 20 hunger points and saturation/exhaustion.
- Health, hunger and air meters in the HUD.
- Underwater air reserve and drowning damage.
- Fall damage, with water cancelling fall damage.
- Lava damage.
- Walking and jumping exhaustion, hunger depletion, regeneration while well-fed, and starvation.
- Death delay and respawn at the world spawn.

### Inventory and blocks
- Stack counts for inventory items; normal items stack to 64 and tools stack to 1.
- Survival inventory is integrated with ClassiCube's existing inventory picker and hotbar.
- Hotbar displays item counts.
- Food can be eaten from the selected hotbar slot.
- Holding the break input builds mining progress; releasing it or changing targets cancels progress.
- Basic block harvest rules and drops for common blocks and ores.
- Broken blocks and defeated mobs create visible dropped-item entities; the player collects them by walking close.
- Dropped stacks use a fixed 32-entity pool to limit memory and CPU use; overflow is sent to inventory when possible.
- Dropped items despawn after five minutes.
- Block placement consumes one item from the inventory.
- Virtual items include food, sticks, coal, ingots, crafting table, furnace, torches, wooden/stone/iron tools and swords, plus mob drops.
- Wooden, stone and iron tools and swords have durability.

### Crafting and smelting
- Crafting uses the /craft chat command and checks for a nearby crafting table/furnace when required.
- Recipes include planks, sticks, crafting table, furnace, torches, wooden/stone/iron tools, swords, iron/gold ingots, and cooked meat.
- Smelting/cooking commands consume one coal immediately; there is not yet a timed furnace interface.

### World and mobs
- A 20-minute day/night cycle adjusts sky, fog and sunlight colours.
- Passive pigs, chickens and sheep spawn when a world loads and wander around.
- Zombies, skeletons, creepers and spiders can spawn at night, pursue the player, and attack at close range.
- Passive mobs drop meat or wool; hostile mobs drop rotten flesh, bones, arrows, gunpowder or string.
- The existing ClassiCube singleplayer Notchy generator is used with a 128-block vertical world on normal builds.

### Saving
- Survival health, hunger, air, inventory stacks, tool durability, hotbar layout, day/night time and up to 32 dropped item stacks are stored in a companion .survival file beside the map.
- The save sidecar is version 4 and retains readers for sidecar versions 1–3.
- The map's own ClassiCube save format is unchanged.

## Current limitations — not yet a complete Beta 1.7.3 recreation

- Worlds are finite ClassiCube maps (normally 128 × 128 horizontally and 128 blocks high), not infinite Beta terrain.
- The Notchy generator is ClassiCube's existing generator, not a faithful Beta 1.7.3 biome/terrain generator.
- The world-drop system is intentionally capped at 32 simultaneous stacks and drops despawn after five minutes; if the pool is full, new drops are put directly into inventory where possible.
- Crafting is command-based rather than a Minecraft crafting-grid screen.
- Furnace recipes are instant commands rather than a burning fuel slot, progress bar and output-slot furnace.
- Mobs use lightweight basic wandering/chasing and proximity attacks. There is no cow, breeding, projectile combat, creeper explosion, armor, shields, enchantments, or full loot tables yet.
- Death currently respawns the player without a Minecraft-style world item-drop/death chest system.
- HUD meters are simple drawn bars, not the original heart, drumstick and bubble icons.
- This code has not yet been successfully compiled or play-tested in this environment. It must pass the 32-bit Windows build and in-game tests before being called a finished playable release.

## Build status

The branch has a Windows GitHub Actions workflow trigger configured, but GitHub currently reports no workflow runs for this branch. There is no verified Windows executable yet.
