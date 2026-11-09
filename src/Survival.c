#include "Survival.h"
#include "BlockID.h"
#include "Block.h"
#include "Commands.h"
#include "Chat.h"
#include "Entity.h"
#include "Model.h"
#include "EntityComponents.h"
#include "ExtMath.h"
#include "Game.h"
#include "Inventory.h"
#include "Event.h"
#include "Errors.h"
#include "String_.h"
#include "Platform.h"
#include "Stream.h"
#include "Graphics.h"
#include "Funcs.h"
#include "Picking.h"
#include "Server.h"
#include "World.h"

#define SURVIVAL_MAX_HEALTH 20
#define SURVIVAL_MAX_FOOD   20
#define SURVIVAL_MAX_AIR    15.0f
#define SURVIVAL_INVENTORY_STACK_SIZE 64
#define SURVIVAL_INVENTORY_SLOTS 36
#define SURVIVAL_TICK       0.5f

static int health, food;
static float saturation, exhaustion, airSeconds;
static float drowningTimer, hazardTimer, starvationTimer, regenerationTimer, deathTimer;
static cc_bool fallActive, hasLastPosition, jumpWasRising;
static float fallDistance, fallLastY, lastX, lastZ;
static struct ScheduledTask2 survivalTask;
static float hostileSpawnTimer;
static int itemCounts[BLOCK_COUNT];
static int toolDurability[BLOCK_COUNT];
static RNGState survivalRng;
static cc_bool miningActive;
static int miningX, miningY, miningZ;
static void SurvivalItem_Tick(struct Entity* e, float delta);
static void Survival_SpawnInitialAnimals(void);
static float timeOfDay;
static int dayCycleStep;
static BlockID miningBlock;
static float miningProgress;

#define SURVIVAL_CROP_SLOTS 512
#define SURVIVAL_CROP_GROW_TIME 90.0f
struct SurvivalCrop {
	int x, y, z;
	float growthTimer, hydrationTimer;
	cc_bool hydrated, active;
};
static struct SurvivalCrop survivalCrops[SURVIVAL_CROP_SLOTS];
static void Survival_UseToolDurability(BlockID block);
static void Survival_CropTick(float delta);
static void Survival_CropRebuild(void);


#ifndef CC_BUILD_TINYMEM
static void Survival_DefineVirtualItem(BlockID id, const char* name, BlockID templateBlock, cc_bool placeable, cc_bool solid) {
	cc_string label = String_FromReadonly(name);
	int face;
	if (Block_IsCustomDefined(id)) Block_UndefineCustom(id);
	Block_ResetProps(id);
	Block_SetName(id, &label);
	for (face = 0; face < FACE_COUNT; face++) {
		Block_Tex(id, face) = Block_Tex(templateBlock, face);
	}
	/* Preserve the template's light-emission and sprite properties. In particular,
	   the crafted torch uses BLOCK_FIRE as its visual/light template. */
	Blocks.Brightness[id] = Blocks.Brightness[templateBlock];
	Blocks.BlocksLight[id] = Blocks.BlocksLight[templateBlock];
	Blocks.SpriteOffset[id] = Blocks.SpriteOffset[templateBlock];
	Blocks.Draw[id] = solid ? DRAW_OPAQUE : DRAW_SPRITE;
	Blocks.Collide[id] = solid ? COLLIDE_SOLID : COLLIDE_NONE;
	Block_DefineCustom(id, true);
	Blocks.CanPlace[id] = placeable;
	Blocks.CanDelete[id] = placeable;
}

static void Survival_DefineVirtualItems(void) {
	Survival_DefineVirtualItem(BLOCK_ITEM_APPLE, "Apple", BLOCK_ROSE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_STICK, "Stick", BLOCK_LOG, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_BREAD, "Bread", BLOCK_SAND, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_RAW_PORK, "Raw pork", BLOCK_RED, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_COOKED_PORK, "Cooked pork", BLOCK_BRICK, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_RAW_BEEF, "Raw beef", BLOCK_RED, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_COOKED_BEEF, "Cooked beef", BLOCK_BRICK, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_RAW_CHICKEN, "Raw chicken", BLOCK_YELLOW, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_COOKED_CHICKEN, "Cooked chicken", BLOCK_SAND, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_CRAFTING_TABLE, "Crafting table", BLOCK_CRATE, true, true);
	Survival_DefineVirtualItem(BLOCK_ITEM_FURNACE, "Furnace", BLOCK_COBBLE, true, true);
	Survival_DefineVirtualItem(BLOCK_ITEM_TORCH, "Torch", BLOCK_FIRE, true, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_WOOD_PICKAXE, "Wooden pickaxe", BLOCK_WOOD, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_STONE_PICKAXE, "Stone pickaxe", BLOCK_STONE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_WOOD_AXE, "Wooden axe", BLOCK_WOOD, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_STONE_AXE, "Stone axe", BLOCK_STONE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_WOOD_SHOVEL, "Wooden shovel", BLOCK_WOOD, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_STONE_SHOVEL, "Stone shovel", BLOCK_STONE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_WHEAT, "Wheat", BLOCK_YELLOW, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_SEEDS, "Seeds", BLOCK_SAPLING, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_COAL, "Coal", BLOCK_BLACK, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_IRON_ORE, "Iron ore", BLOCK_IRON_ORE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_GOLD_ORE, "Gold ore", BLOCK_GOLD_ORE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_IRON_INGOT, "Iron ingot", BLOCK_IRON, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_GOLD_INGOT, "Gold ingot", BLOCK_GOLD, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_ROTTEN_FLESH, "Rotten flesh", BLOCK_BROWN, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_BONE, "Bone", BLOCK_WHITE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_ARROW, "Arrow", BLOCK_GRAY, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_GUNPOWDER, "Gunpowder", BLOCK_BLACK, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_STRING, "String", BLOCK_WHITE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_WOOD_SWORD, "Wooden sword", BLOCK_WOOD, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_STONE_SWORD, "Stone sword", BLOCK_STONE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_IRON_PICKAXE, "Iron pickaxe", BLOCK_IRON, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_IRON_AXE, "Iron axe", BLOCK_IRON, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_IRON_SHOVEL, "Iron shovel", BLOCK_IRON, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_IRON_SWORD, "Iron sword", BLOCK_IRON, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_FARMLAND, "Farmland", BLOCK_DIRT, true, true);
	Survival_DefineVirtualItem(BLOCK_ITEM_CROP_0, "Wheat crop", BLOCK_SAPLING, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_CROP_1, "Growing wheat", BLOCK_SAPLING, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_CROP_2, "Growing wheat", BLOCK_SAPLING, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_CROP_3, "Growing wheat", BLOCK_SAPLING, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_CROP_4, "Growing wheat", BLOCK_DANDELION, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_CROP_5, "Growing wheat", BLOCK_DANDELION, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_CROP_6, "Growing wheat", BLOCK_DANDELION, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_CROP_7, "Mature wheat", BLOCK_DANDELION, false, false);
	Blocks.CanDelete[BLOCK_ITEM_CROP_0] = true;
	Blocks.CanDelete[BLOCK_ITEM_CROP_1] = true;
	Blocks.CanDelete[BLOCK_ITEM_CROP_2] = true;
	Blocks.CanDelete[BLOCK_ITEM_CROP_3] = true;
	Blocks.CanDelete[BLOCK_ITEM_CROP_4] = true;
	Blocks.CanDelete[BLOCK_ITEM_CROP_5] = true;
	Blocks.CanDelete[BLOCK_ITEM_CROP_6] = true;
	Blocks.CanDelete[BLOCK_ITEM_CROP_7] = true;
	Survival_DefineVirtualItem(BLOCK_ITEM_WOOD_HOE, "Wooden hoe", BLOCK_WOOD, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_STONE_HOE, "Stone hoe", BLOCK_STONE, false, false);
	Survival_DefineVirtualItem(BLOCK_ITEM_IRON_HOE, "Iron hoe", BLOCK_IRON, false, false);
}
#endif

cc_bool Survival_IsActive(void) {
#if defined CC_BUILD_TINYMEM
	return false;
#else
	return Server.IsSinglePlayer && !Game_ClassicMode && World.Loaded;
#endif
}
#define Survival_Active Survival_IsActive

static cc_bool Survival_IsTool(BlockID block) {
	return block == BLOCK_ITEM_WOOD_PICKAXE || block == BLOCK_ITEM_STONE_PICKAXE || block == BLOCK_ITEM_IRON_PICKAXE ||
		block == BLOCK_ITEM_WOOD_AXE || block == BLOCK_ITEM_STONE_AXE || block == BLOCK_ITEM_IRON_AXE ||
		block == BLOCK_ITEM_WOOD_SHOVEL || block == BLOCK_ITEM_STONE_SHOVEL || block == BLOCK_ITEM_IRON_SHOVEL ||
		block == BLOCK_ITEM_WOOD_SWORD || block == BLOCK_ITEM_STONE_SWORD || block == BLOCK_ITEM_IRON_SWORD ||
		block == BLOCK_ITEM_WOOD_HOE || block == BLOCK_ITEM_STONE_HOE || block == BLOCK_ITEM_IRON_HOE;
}

static int Survival_ToolMaxDurability(BlockID block) {
	switch (block) {
	case BLOCK_ITEM_WOOD_PICKAXE: case BLOCK_ITEM_WOOD_AXE: case BLOCK_ITEM_WOOD_SHOVEL: case BLOCK_ITEM_WOOD_SWORD: case BLOCK_ITEM_WOOD_HOE: return 60;
	case BLOCK_ITEM_STONE_PICKAXE: case BLOCK_ITEM_STONE_AXE: case BLOCK_ITEM_STONE_SHOVEL: case BLOCK_ITEM_STONE_SWORD: case BLOCK_ITEM_STONE_HOE: return 132;
	case BLOCK_ITEM_IRON_PICKAXE: case BLOCK_ITEM_IRON_AXE: case BLOCK_ITEM_IRON_SHOVEL: case BLOCK_ITEM_IRON_SWORD: case BLOCK_ITEM_IRON_HOE: return 251;
	default: return 0;
	}
}

static int Survival_ItemMaxStack(BlockID block) {
	return Survival_IsTool(block) ? 1 : SURVIVAL_INVENTORY_STACK_SIZE;
}

static int Survival_UsedInventoryStacks(void) {
	int i, slots = 0;
	for (i = 1; i < BLOCK_COUNT; i++) if (itemCounts[i] > 0) slots += (itemCounts[i] + Survival_ItemMaxStack((BlockID)i) - 1) / Survival_ItemMaxStack((BlockID)i);
	return slots;
}

static void Survival_RefreshInventoryMap(void) {
	int i;
	for (i = 0; i < BLOCK_COUNT; i++) Inventory.Map[i] = BLOCK_AIR;
	for (i = 1; i < BLOCK_COUNT; i++) {
		if (itemCounts[i] > 0) Inventory.Map[i - 1] = (BlockID)i;
	}
}

int Survival_GetItemCount(BlockID block) {
	if (!Survival_IsActive() || block <= BLOCK_AIR || block >= BLOCK_COUNT) return 0;
	return itemCounts[block];
}

int Survival_InventoryAdd(BlockID block, int amount) {
	int used, freeSlots, room, capacity, maxStack, i;
	if (!Survival_IsActive() || block <= BLOCK_AIR || block >= BLOCK_COUNT || amount <= 0) return 0;
	maxStack = Survival_ItemMaxStack(block);
	if (maxStack == 1 && itemCounts[block] > 0) return 0;
	used = Survival_UsedInventoryStacks();
	freeSlots = SURVIVAL_INVENTORY_SLOTS - used;
	if (freeSlots < 0) freeSlots = 0;
	room = itemCounts[block] % maxStack;
	room = room ? maxStack - room : 0;
	capacity = maxStack == 1 ? (freeSlots > 0 ? 1 : 0) : room + freeSlots * maxStack;
	if (amount > capacity) amount = capacity;
	if (amount <= 0) return 0;

	itemCounts[block] += amount;
	if (Survival_IsTool(block)) toolDurability[block] = Survival_ToolMaxDurability(block);
	Survival_RefreshInventoryMap();
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		if (Inventory_Get(i) == block) return amount;
	}
	for (i = 0; i < INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		if (Inventory_Get(i) != BLOCK_AIR) continue;
		Inventory_Set(i, block);
		Event_RaiseVoid(&UserEvents.HeldBlockChanged);
		break;
	}
	return amount;
}

static int Survival_InventoryRemove(BlockID block, int amount) {
	int i, removed, wasSelected;
	if (!Survival_IsActive() || block <= BLOCK_AIR || block >= BLOCK_COUNT || amount <= 0) return 0;
	if (itemCounts[block] <= 0) return 0;
	removed = min(amount, itemCounts[block]);
	itemCounts[block] -= removed;
	if (itemCounts[block] == 0) {
		toolDurability[block] = 0;
		wasSelected = Inventory_SelectedBlock == block;
		for (i = 0; i < INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR; i++) {
			if (Inventory.Table[i] == block) Inventory.Table[i] = BLOCK_AIR;
		}
		if (wasSelected) Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	}
	Survival_RefreshInventoryMap();
	return removed;
}

cc_bool Survival_CanPlaceBlock(BlockID block) {
	if (!Survival_IsActive()) return true;
	return block > BLOCK_AIR && block < BLOCK_COUNT && itemCounts[block] > 0;
}

void Survival_OnBlockPlaced(BlockID block) {
	if (Survival_IsActive()) Survival_InventoryRemove(block, 1);
}

#define SURVIVAL_ITEM_SLOTS 40
#define SURVIVAL_ITEM_ENTITY_BASE 16

struct SurvivalItem {
	struct Entity Base;
	struct HacksComp Hacks;
	struct PhysicsComp Physics;
	struct CollisionsComp Collisions;
	int entityID;
	BlockID block;
	int count;
	int durability;
	float age, pickupDelay;
	cc_bool active;
};

static struct SurvivalItem survivalItems[SURVIVAL_ITEM_SLOTS];
static void SurvivalItem_ClearAll(void);

static struct SurvivalItem* SurvivalItem_FindFree(void) {
	int i;
	for (i = 0; i < SURVIVAL_ITEM_SLOTS; i++) {
		int id = SURVIVAL_ITEM_ENTITY_BASE + i;
		if (!survivalItems[i].active && !Entities.List[id]) return &survivalItems[i];
	}
	return NULL;
}

static int SurvivalItem_FindID(struct SurvivalItem* item) {
	int i;
	for (i = 0; i < SURVIVAL_ITEM_SLOTS; i++) {
		if (&survivalItems[i] == item) return SURVIVAL_ITEM_ENTITY_BASE + i;
	}
	return -1;
}

static PackedCol SurvivalItem_GetColor(struct Entity* e) {
	(void)e;
	return PACKEDCOL_WHITE;
}

static void SurvivalItem_Despawn(struct Entity* e) {
	struct SurvivalItem* item = (struct SurvivalItem*)e;
	if (e->Flags & ENTITY_FLAG_HAS_MODELVB) Gfx_DeleteScratchVb(&e->ModelVB);
	item->active = false;
}

static void SurvivalItem_SetLocation(struct Entity* e, struct LocationUpdate* update) {
	if (update->flags & LU_HAS_POS) {
		e->Position = update->pos;
		e->prev.pos = update->pos;
		e->next.pos = update->pos;
	}
	if (update->flags & LU_HAS_YAW) e->Yaw = update->yaw;
	if (update->flags & LU_HAS_PITCH) e->Pitch = update->pitch;
}

static void SurvivalItem_RenderModel(struct Entity* e, float delta, float t) {
	(void)delta;
	Vec3_Lerp(&e->Position, &e->prev.pos, &e->next.pos, t);
	e->ShouldRender = Model_ShouldRender(e);
	if (e->ShouldRender) Model_Render(e->Model, e);
}

static cc_bool SurvivalItem_ShouldRenderName(struct Entity* e) {
	(void)e;
	return false;
}

static const struct EntityVTABLE survivalItem_VTABLE = {
	SurvivalItem_Tick, SurvivalItem_Despawn, SurvivalItem_SetLocation, SurvivalItem_GetColor,
	SurvivalItem_RenderModel, SurvivalItem_ShouldRenderName
};

static void SurvivalItem_Tick(struct Entity* e, float delta) {
	struct SurvivalItem* item = (struct SurvivalItem*)e;
	struct LocalPlayer* player = Entities.CurPlayer;
	Vec3 oldPos = e->Position;
	Vec3 movement = { 0.0f, 0.0f, 0.0f };
	float dx, dy, dz, distanceSq;
	int added;

	if (!item->active || !Survival_IsActive() || !World.Loaded) return;
	item->age += delta;
	item->pickupDelay -= delta;
	if (item->age >= 300.0f || e->Position.y < -5.0f || Entity_TouchesAnyLava(e)) {
		Entities_Remove(item->entityID);
		return;
	}

	PhysicsComp_UpdateVelocityState(&item->Physics);
	PhysicsComp_PhysicsTick(&item->Physics, movement);
	e->prev.pos = oldPos;
	e->next.pos = e->Position;
	e->Yaw = Math_ClampAngle(e->Yaw + delta * 90.0f);
	e->Anim.BobbingModel = Math_SinF((float)Game.Time + item->entityID) * 0.045f;

	if (!player || Survival_IsDead() || item->pickupDelay > 0.0f) return;
	dx = player->Base.Position.x - e->Position.x;
	dy = player->Base.Position.y - e->Position.y;
	dz = player->Base.Position.z - e->Position.z;
	distanceSq = dx * dx + dy * dy + dz * dz;
	if (distanceSq > 1.75f * 1.75f) return;

	{
		BlockID block = item->block;
		int durability = item->durability;
		added = Survival_InventoryAdd(block, item->count);
		if (added > 0 && Survival_IsTool(block) && durability > 0) toolDurability[block] = durability;
	}
	item->count -= added;
	if (item->count <= 0) Entities_Remove(item->entityID);
}

static void SurvivalItem_SpawnEx(BlockID block, int amount, Vec3 position, int durability) {
	static const cc_string blockModel = String_FromConst("block");
	int remaining = amount, i;

	if (!Survival_IsActive() || block <= BLOCK_AIR || block >= BLOCK_COUNT || amount <= 0) return;

	/* Merge nearby stacks before consuming another entity slot. The pool is
	   deliberately small for low-memory singleplayer targets. */
	for (i = 0; i < SURVIVAL_ITEM_SLOTS && remaining > 0; i++) {
		struct SurvivalItem* existing = &survivalItems[i];
		float dx, dy, dz;
		int room, merged;
		if (!existing->active || existing->block != block || existing->count >= SURVIVAL_INVENTORY_STACK_SIZE ||
			Survival_ItemMaxStack(block) <= 1 || existing->durability != durability) continue;
		dx = existing->Base.Position.x - position.x;
		dy = existing->Base.Position.y - position.y;
		dz = existing->Base.Position.z - position.z;
		if (dx * dx + dy * dy + dz * dz > 1.0f) continue;
		room = SURVIVAL_INVENTORY_STACK_SIZE - existing->count;
		merged = min(room, remaining);
		existing->count += merged;
		remaining -= merged;
	}
	while (remaining > 0) {
		struct SurvivalItem* item = SurvivalItem_FindFree();
		int id, count;
		if (!item) {
			int added = Survival_InventoryAdd(block, remaining);
			if (added > 0 && Survival_IsTool(block) && durability > 0) toolDurability[block] = durability;
			if (added < remaining) Chat_AddRaw("&eThere is no room for more dropped items.");
			return;
		}

		id = SurvivalItem_FindID(item);
		if (id < 0) return;
		count = min(remaining, SURVIVAL_INVENTORY_STACK_SIZE);
		Mem_Set(item, 0, sizeof(*item));
		item->entityID = id;
		item->block = block;
		item->count = count;
		item->durability = durability;
		item->pickupDelay = 0.45f;
		item->active = true;

		Entity_Init(&item->Base);
		Entity_SetModel(&item->Base, &blockModel);
		item->Base.ModelBlock = block;
		Entity_UpdateModelBounds(&item->Base);
		item->Base.VTABLE = &survivalItem_VTABLE;
		AnimatedComp_Init(&item->Base.Anim);

		HacksComp_Init(&item->Hacks);
		item->Hacks.CanBePushed = false;
		item->Hacks.Floating = false;
		PhysicsComp_Init(&item->Physics, &item->Base);
		item->Physics.Hacks = &item->Hacks;
		item->Physics.Collisions = &item->Collisions;
		item->Collisions.Entity = &item->Base;
		item->Collisions.StepSize = 0.5f;

		/* position is already the final world-space item origin. This also
		   prevents saved drops rising by another quarter block on each reload. */
		item->Base.Position = position;
		item->Base.prev.pos = item->Base.Position;
		item->Base.next.pos = item->Base.Position;
		item->Base.Yaw = (float)Random_Next(&survivalRng, 360);
		item->Base.Velocity.x = ((float)Random_Next(&survivalRng, 200) - 100.0f) / 1000.0f;
		item->Base.Velocity.y = 0.12f;
		item->Base.Velocity.z = ((float)Random_Next(&survivalRng, 200) - 100.0f) / 1000.0f;
		Entities.List[id] = &item->Base;
		remaining -= count;
	}
}

static void SurvivalItem_Spawn(BlockID block, int amount, Vec3 position) {
	SurvivalItem_SpawnEx(block, amount, position, 0);
}

static void SurvivalItem_ClearAll(void) {
	int i;
	for (i = 0; i < SURVIVAL_ITEM_SLOTS; i++) {
		if (survivalItems[i].active) Entities_Remove(survivalItems[i].entityID);
	}
}

static void Survival_DropInventory(void) {
	int drops[BLOCK_COUNT];
	int dropDurability[BLOCK_COUNT];
	int i;
	Vec3 position;
	if (!Survival_IsActive() || !Entities.CurPlayer) return;

	Mem_Copy(drops, itemCounts, sizeof(drops));
	Mem_Copy(dropDurability, toolDurability, sizeof(dropDurability));
	Mem_Set(itemCounts, 0, sizeof(itemCounts));
	Mem_Set(toolDurability, 0, sizeof(toolDurability));
	for (i = 0; i < INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		Inventory.Table[i] = BLOCK_AIR;
	}
	Survival_RefreshInventoryMap();
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);

	position = Entities.CurPlayer->Base.Position;
	position.y += 0.25f;
	for (i = 1; i < BLOCK_COUNT; i++) {
		if (drops[i] > 0) {
			SurvivalItem_SpawnEx((BlockID)i, drops[i], position,
				Survival_IsTool((BlockID)i) ? dropDurability[i] : 0);
		}
	}
	Chat_AddRaw("&cYou dropped your inventory.");
}



static cc_bool Survival_IsCrop(BlockID block) {
	return block >= BLOCK_ITEM_CROP_0 && block <= BLOCK_ITEM_CROP_7;
}

static cc_bool Survival_CropRegister(int x, int y, int z) {
	int i;
	for (i = 0; i < SURVIVAL_CROP_SLOTS; i++) {
		struct SurvivalCrop* crop = &survivalCrops[i];
		if (crop->active && crop->x == x && crop->y == y && crop->z == z) return true;
	}
	for (i = 0; i < SURVIVAL_CROP_SLOTS; i++) {
		struct SurvivalCrop* crop = &survivalCrops[i];
		if (crop->active) continue;
		Mem_Set(crop, 0, sizeof(*crop));
		crop->x = x; crop->y = y; crop->z = z;
		crop->hydrationTimer = 15.0f;
		crop->active = true;
		return true;
	}
	return false;
}

static cc_bool Survival_CropHasWater(int x, int y, int z) {
	int dx, dy, dz;
	for (dy = -1; dy <= 1; dy++) {
		for (dz = -4; dz <= 4; dz++) {
			for (dx = -4; dx <= 4; dx++) {
				BlockID block = World_SafeGetBlock(x + dx, y + dy, z + dz);
				if (block == BLOCK_WATER || block == BLOCK_STILL_WATER) return true;
			}
		}
	}
	return false;
}

static cc_bool Survival_CropHasSky(int x, int y, int z) {
	for (y++; y < World.Height; y++) {
		BlockID block = World_SafeGetBlock(x, y, z);
		if (Blocks.BlocksLight[block]) return false;
	}
	return true;
}

static void Survival_CropTick(float delta) {
	int i;
	for (i = 0; i < SURVIVAL_CROP_SLOTS; i++) {
		struct SurvivalCrop* crop = &survivalCrops[i];
		BlockID block;
		IVec3 pos;
		if (!crop->active) continue;
		block = World_SafeGetBlock(crop->x, crop->y, crop->z);
		if (!Survival_IsCrop(block) ||
			World_SafeGetBlock(crop->x, crop->y - 1, crop->z) != BLOCK_ITEM_FARMLAND) {
			crop->active = false;
			continue;
		}
		if (block == BLOCK_ITEM_CROP_7) continue;

		crop->hydrationTimer -= delta;
		if (crop->hydrationTimer <= 0.0f) {
			crop->hydrated = Survival_CropHasWater(crop->x, crop->y - 1, crop->z);
			crop->hydrationTimer = 15.0f;
		}
		if (!Survival_CropHasSky(crop->x, crop->y, crop->z)) continue;

		crop->growthTimer += delta * (crop->hydrated ? 1.0f : 0.5f);
		if (crop->growthTimer < SURVIVAL_CROP_GROW_TIME) continue;
		crop->growthTimer = 0.0f;
		pos.x = crop->x; pos.y = crop->y; pos.z = crop->z;
		Game_ChangeBlock(crop->x, crop->y, crop->z, (BlockID)(block + 1));
		Event_RaiseBlock(&UserEvents.BlockChanged, pos, block, (BlockID)(block + 1));
	}
}

static void Survival_CropRebuild(void) {
	int x, y, z;
	Mem_Set(survivalCrops, 0, sizeof(survivalCrops));
	for (y = 0; y < World.Height; y++) {
		for (z = 0; z < World.Length; z++) {
			for (x = 0; x < World.Width; x++) {
				if (Survival_IsCrop(World_GetBlock(x, y, z))) Survival_CropRegister(x, y, z);
			}
		}
	}
}

static cc_bool Survival_PlantSeeds(void) {
	IVec3 soil, cropPos;
	if (!Game_SelectedPos.valid) return true;
	soil = Game_SelectedPos.pos;
	cropPos = Game_SelectedPos.translatedPos;
	if (World_SafeGetBlock(soil.x, soil.y, soil.z) != BLOCK_ITEM_FARMLAND ||
		cropPos.x != soil.x || cropPos.z != soil.z || cropPos.y != soil.y + 1 ||
		!World_Contains(cropPos.x, cropPos.y, cropPos.z) ||
		World_GetBlock(cropPos.x, cropPos.y, cropPos.z) != BLOCK_AIR) {
		Chat_AddRaw("&ePlant seeds on top of farmland.");
		return true;
	}
	if (Survival_GetItemCount(BLOCK_ITEM_SEEDS) <= 0) {
		Chat_AddRaw("&eYou need seeds to plant crops.");
		return true;
	}
	if (!Survival_CropRegister(cropPos.x, cropPos.y, cropPos.z)) {
		Chat_AddRaw("&eToo many crops are planted in this world.");
		return true;
	}
	if (!Survival_InventoryRemove(BLOCK_ITEM_SEEDS, 1)) return true;
	Game_ChangeBlock(cropPos.x, cropPos.y, cropPos.z, BLOCK_ITEM_CROP_0);
	Event_RaiseBlock(&UserEvents.BlockChanged, cropPos, BLOCK_AIR, BLOCK_ITEM_CROP_0);
	return true;
}

static void Survival_ConsumeHoeDurability(BlockID hoe) {
	if (toolDurability[hoe] <= 0) return;
	toolDurability[hoe]--;
	if (toolDurability[hoe] <= 0) {
		Chat_AddRaw("&cYour hoe broke.");
		Survival_InventoryRemove(hoe, 1);
	}
}

static cc_bool Survival_UseHoe(BlockID hoe) {
	IVec3 pos, above;
	BlockID old;
	if (!Game_SelectedPos.valid) return true;
	pos = Game_SelectedPos.pos;
	if (!World_Contains(pos.x, pos.y, pos.z)) return true;
	old = World_GetBlock(pos.x, pos.y, pos.z);
	above.x = pos.x; above.y = pos.y + 1; above.z = pos.z;
	if ((old != BLOCK_GRASS && old != BLOCK_DIRT) || !World_Contains(above.x, above.y, above.z) ||
		World_GetBlock(above.x, above.y, above.z) != BLOCK_AIR) return true;

	Game_ChangeBlock(pos.x, pos.y, pos.z, BLOCK_ITEM_FARMLAND);
	Event_RaiseBlock(&UserEvents.BlockChanged, pos, old, BLOCK_ITEM_FARMLAND);
	Survival_ConsumeHoeDurability(hoe);
	return true;
}

static void Survival_UseToolDurability(BlockID block);

cc_bool Survival_UseItem(BlockID block) {
	int points = 0;
	float sat = 0.0f;
	if (!Survival_IsActive()) return false;
	if (block == BLOCK_ITEM_SEEDS) return Survival_PlantSeeds();
	if (block == BLOCK_ITEM_WOOD_HOE || block == BLOCK_ITEM_STONE_HOE || block == BLOCK_ITEM_IRON_HOE) return Survival_UseHoe(block);

	switch (block) {
	case BLOCK_ITEM_APPLE: points = 4; sat = 2.4f; break;
	case BLOCK_ITEM_BREAD: points = 5; sat = 6.0f; break;
	case BLOCK_ITEM_RAW_PORK: points = 3; sat = 1.8f; break;
	case BLOCK_ITEM_ROTTEN_FLESH: points = 2; sat = 0.4f; break;
	case BLOCK_ITEM_COOKED_PORK: points = 8; sat = 12.8f; break;
	case BLOCK_ITEM_RAW_BEEF: points = 3; sat = 1.8f; break;
	case BLOCK_ITEM_COOKED_BEEF: points = 8; sat = 12.8f; break;
	case BLOCK_ITEM_RAW_CHICKEN: points = 2; sat = 1.2f; break;
	case BLOCK_ITEM_COOKED_CHICKEN: points = 6; sat = 7.2f; break;
	default: return false;
	}

	if (food >= SURVIVAL_MAX_FOOD) {
		Chat_AddRaw("&eYour hunger bar is already full.");
		return true;
	}
	if (!Survival_InventoryRemove(block, 1)) return true;
	Survival_AddFood(points, sat);
	Chat_AddRaw("&aYou ate food.");
	return true;
}


static cc_bool Survival_HasPickaxe(void);
static cc_bool Survival_HasStonePickaxe(void);

static float Survival_GetBlockMiningTime(BlockID block) {
	BlockID held = Inventory_SelectedBlock;
	float time;
	switch (block) {
	case BLOCK_OBSIDIAN: time = 8.0f; break;
	case BLOCK_STONE: case BLOCK_COBBLE: case BLOCK_GOLD_ORE: case BLOCK_IRON_ORE:
	case BLOCK_COAL_ORE: case BLOCK_BRICK: case BLOCK_STONE_BRICK: case BLOCK_MOSSY_ROCKS:
	case BLOCK_SANDSTONE: case BLOCK_GOLD: case BLOCK_IRON:
		time = Survival_HasPickaxe() ? 1.8f : 5.0f;
		if (held == BLOCK_ITEM_STONE_PICKAXE) time *= 0.55f;
		if (held == BLOCK_ITEM_IRON_PICKAXE) time *= 0.40f;
		break;
	case BLOCK_LOG: case BLOCK_WOOD: case BLOCK_BOOKSHELF: case BLOCK_CRATE:
		time = 1.0f;
		if ((held == BLOCK_ITEM_WOOD_AXE || held == BLOCK_ITEM_STONE_AXE || held == BLOCK_ITEM_IRON_AXE) && toolDurability[held] > 0) time *= 0.35f;
		break;
	case BLOCK_DIRT: case BLOCK_GRASS: case BLOCK_SAND: case BLOCK_GRAVEL: case BLOCK_SNOW:
		time = 0.5f;
		if ((held == BLOCK_ITEM_WOOD_SHOVEL || held == BLOCK_ITEM_STONE_SHOVEL || held == BLOCK_ITEM_IRON_SHOVEL) && toolDurability[held] > 0) time *= 0.35f;
		break;
	case BLOCK_LEAVES: case BLOCK_SAPLING: case BLOCK_DANDELION: case BLOCK_ROSE:
	case BLOCK_BROWN_SHROOM: case BLOCK_RED_SHROOM:
		time = 0.5f;
		if ((held == BLOCK_ITEM_WOOD_AXE || held == BLOCK_ITEM_STONE_AXE || held == BLOCK_ITEM_IRON_AXE) && toolDurability[held] > 0) time *= 0.4f;
		break;
	default: time = 1.0f; break;
	}
	return time;
}

void Survival_CancelMining(void) {
	miningActive = false;
	miningProgress = 0.0f;
}

float Survival_GetMiningProgress(void) {
	return miningActive ? min(1.0f, miningProgress / Survival_GetBlockMiningTime(miningBlock)) : 0.0f;
}

cc_bool Survival_MineBlock(int x, int y, int z, BlockID block, float elapsed) {
	float required;
	if (!Survival_IsActive()) return true;

	if (!miningActive || miningX != x || miningY != y || miningZ != z || miningBlock != block) {
		miningActive = true;
		miningX = x; miningY = y; miningZ = z;
		miningBlock = block;
		miningProgress = 0.0f;
	}

	/* InputHandler_Tick supplies elapsed time while the button is held; press-only calls start mining. */
	if (elapsed > 0.0f) miningProgress += elapsed;
	required = Survival_GetBlockMiningTime(block);
	if (miningProgress < required) return false;

	Survival_CancelMining();
	return true;
}

static cc_bool Survival_HasPickaxe(void) {
	BlockID held = Inventory_SelectedBlock;
	return (held == BLOCK_ITEM_WOOD_PICKAXE || held == BLOCK_ITEM_STONE_PICKAXE || held == BLOCK_ITEM_IRON_PICKAXE) && toolDurability[held] > 0;
}

static cc_bool Survival_HasStonePickaxe(void) {
	BlockID held = Inventory_SelectedBlock;
	return (held == BLOCK_ITEM_STONE_PICKAXE || held == BLOCK_ITEM_IRON_PICKAXE) && toolDurability[held] > 0;
}

static cc_bool Survival_HasIronPickaxe(void) {
	BlockID held = Inventory_SelectedBlock;
	return held == BLOCK_ITEM_IRON_PICKAXE && toolDurability[held] > 0;
}

static void Survival_UseToolDurability(BlockID block) {
	BlockID held = Inventory_SelectedBlock;
	cc_bool usesTool = false;
	if (toolDurability[held] <= 0) return;

	switch (block) {
	case BLOCK_STONE: case BLOCK_COBBLE: case BLOCK_COAL_ORE:
	case BLOCK_IRON_ORE: case BLOCK_GOLD_ORE: case BLOCK_OBSIDIAN:
	case BLOCK_BRICK: case BLOCK_STONE_BRICK: case BLOCK_MOSSY_ROCKS:
	case BLOCK_SANDSTONE: case BLOCK_GOLD: case BLOCK_IRON:
		usesTool = held == BLOCK_ITEM_WOOD_PICKAXE || held == BLOCK_ITEM_STONE_PICKAXE || held == BLOCK_ITEM_IRON_PICKAXE;
		break;
	case BLOCK_LOG: case BLOCK_WOOD: case BLOCK_BOOKSHELF: case BLOCK_CRATE:
		usesTool = held == BLOCK_ITEM_WOOD_AXE || held == BLOCK_ITEM_STONE_AXE || held == BLOCK_ITEM_IRON_AXE;
		break;
	case BLOCK_DIRT: case BLOCK_GRASS:
		usesTool = held == BLOCK_ITEM_WOOD_SHOVEL || held == BLOCK_ITEM_STONE_SHOVEL || held == BLOCK_ITEM_IRON_SHOVEL;
		break;
	case BLOCK_SAND: case BLOCK_GRAVEL: case BLOCK_SNOW:
		usesTool = held == BLOCK_ITEM_WOOD_SHOVEL || held == BLOCK_ITEM_STONE_SHOVEL || held == BLOCK_ITEM_IRON_SHOVEL;
		break;
	}
	if (!usesTool) return;

	toolDurability[held]--;
	if (toolDurability[held] <= 0) {
		Chat_AddRaw("&cYour tool broke.");
		Survival_InventoryRemove(held, 1);
	}
}

static BlockID Survival_GetBlockDrop(BlockID block) {
	if (block == BLOCK_ITEM_FARMLAND) return BLOCK_DIRT;
	if (block == BLOCK_DANDELION) return BLOCK_ITEM_SEEDS;
	if (block == BLOCK_ITEM_CROP_7) return BLOCK_ITEM_WHEAT;
	if (Survival_IsCrop(block)) return Random_Next(&survivalRng, 3) == 0 ? BLOCK_ITEM_SEEDS : BLOCK_AIR;
	if (block == BLOCK_GRASS) return BLOCK_DIRT;
	if (block == BLOCK_STONE) return Survival_HasPickaxe() ? BLOCK_COBBLE : BLOCK_AIR;
	if (block == BLOCK_COBBLE || block == BLOCK_BRICK || block == BLOCK_STONE_BRICK ||
		block == BLOCK_MOSSY_ROCKS || block == BLOCK_SANDSTONE) return Survival_HasPickaxe() ? block : BLOCK_AIR;
	if (block == BLOCK_COAL_ORE) return Survival_HasPickaxe() ? BLOCK_ITEM_COAL : BLOCK_AIR;
	if (block == BLOCK_IRON_ORE) return Survival_HasStonePickaxe() ? BLOCK_ITEM_IRON_ORE : BLOCK_AIR;
	if (block == BLOCK_GOLD_ORE) return Survival_HasIronPickaxe() ? BLOCK_ITEM_GOLD_ORE : BLOCK_AIR;
	if (block == BLOCK_GOLD || block == BLOCK_IRON) return Survival_HasIronPickaxe() ? block : BLOCK_AIR;
	if (block == BLOCK_OBSIDIAN) return BLOCK_AIR;
	if (block == BLOCK_LEAVES) {
		if (Random_Next(&survivalRng, 200) == 0) return BLOCK_ITEM_APPLE;
		if (Random_Next(&survivalRng, 20) == 0) return BLOCK_SAPLING;
		return BLOCK_AIR;
	}
	if (block == BLOCK_AIR || block == BLOCK_WATER || block == BLOCK_STILL_WATER ||
		block == BLOCK_LAVA || block == BLOCK_STILL_LAVA || block == BLOCK_BEDROCK) return BLOCK_AIR;
	return block;
}

void Survival_OnBlockBroken(BlockID block, int x, int y, int z) {
	BlockID drop;
	Vec3 position;
	if (!Survival_IsActive()) return;
	drop = Survival_GetBlockDrop(block);
	Survival_UseToolDurability(block);
	if (drop == BLOCK_AIR) return;
	position.x = x + 0.5f;
	position.y = y + 0.5f;
	position.z = z + 0.5f;
	SurvivalItem_Spawn(drop, 1, position);
	if (block == BLOCK_ITEM_CROP_7) {
		SurvivalItem_Spawn(BLOCK_ITEM_SEEDS, 1 + Random_Next(&survivalRng, 3), position);
	}
}

static void SurvivalMob_ClearAll(void);
static void SurvivalItem_ClearAll(void);
static cc_bool Survival_IsNight(void);
static int SurvivalMob_Count(cc_bool hostileOnly);
static void Survival_SpawnNightMob(void);

static void Survival_OnNewMap(void) {
	int i;
	/* Do not change Classic or multiplayer maps. */
	if (!Server.IsSinglePlayer || Game_ClassicMode) return;
#ifndef CC_BUILD_TINYMEM
	/* Define custom block IDs before the map data is decoded. */
	Survival_DefineVirtualItems();
#endif
	SurvivalMob_ClearAll();
	SurvivalItem_ClearAll();
	hostileSpawnTimer = 0.0f;
	Survival_Reset();
	Mem_Set(survivalCrops, 0, sizeof(survivalCrops));
	timeOfDay = 0.0f;
	dayCycleStep = -1;
	SurvivalItem_ClearAll();
	Mem_Set(itemCounts, 0, sizeof(itemCounts));
	Mem_Set(toolDurability, 0, sizeof(toolDurability));
	for (i = 0; i < INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR; i++) Inventory.Table[i] = BLOCK_AIR;
	Inventory.SelectedIndex = 0;
	Inventory.Offset = 0;
	Survival_RefreshInventoryMap();
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
}


/* Stores Survival state beside the corresponding map as <map>.survival. */
static cc_result Survival_MakeSidecarPath(cc_string* path, char* buffer, const cc_string* mapPath) {
	if (!mapPath || !mapPath->length || mapPath->length + 10 >= FILENAME_SIZE) return ERR_INVALID_ARGUMENT;
	path->buffer = buffer;
	path->length = 0;
	path->capacity = FILENAME_SIZE;
	String_AppendString(path, mapPath);
	String_AppendConst(path, ".survival");
	return 0;
}

cc_result Survival_Save(const cc_string* mapPath) {
	struct Stream stream;
	cc_filepath rawPath;
	cc_string path;
	char pathBuffer[FILENAME_SIZE];
	cc_uint8 data[14 + BLOCK_COUNT * 6 + INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR * 2 + 1 + SURVIVAL_ITEM_SLOTS * 18];
	cc_uint16 dayTicks;
	int i, itemTypes = 0, droppedCount = 0, offset = 14;
	cc_result res;

	if (!Survival_IsActive()) return 0;
	if ((res = Survival_MakeSidecarPath(&path, pathBuffer, mapPath))) return res;

	for (i = 1; i < BLOCK_COUNT; i++) {
		if (itemCounts[i] > 0) itemTypes++;
	}
	data[0] = 'C'; data[1] = 'C'; data[2] = 'S'; data[3] = 'V';
	data[4] = 5;
	data[5] = (cc_uint8)max(0, min(health, SURVIVAL_MAX_HEALTH));
	data[6] = (cc_uint8)max(0, min(food, SURVIVAL_MAX_FOOD));
	data[7] = (cc_uint8)max(0, min((int)(airSeconds + 0.5f), 15));
	data[8] = (cc_uint8)max(0, min((int)(saturation + 0.5f), SURVIVAL_MAX_FOOD));
	data[9] = (cc_uint8)min(itemTypes, SURVIVAL_INVENTORY_SLOTS);
	data[10] = (cc_uint8)max(0, min(Inventory.SelectedIndex, HOTBAR_MAX_INDEX));
	data[11] = (cc_uint8)max(0, min(Inventory.Offset / INVENTORY_BLOCKS_PER_HOTBAR, INVENTORY_HOTBARS - 1));
	dayTicks = (cc_uint16)max(0, min((int)(timeOfDay * 2.0f + 0.5f), 2399));
	data[12] = (cc_uint8)(dayTicks & 0xFF);
	data[13] = (cc_uint8)(dayTicks >> 8);

	itemTypes = 0;
	for (i = 1; i < BLOCK_COUNT && itemTypes < data[9]; i++) {
		cc_uint16 block, count;
		if (itemCounts[i] <= 0) continue;
		block = (cc_uint16)i;
		count = (cc_uint16)min(itemCounts[i], SURVIVAL_INVENTORY_SLOTS * Survival_ItemMaxStack((BlockID)i));
		data[offset++] = (cc_uint8)(block & 0xFF);
		data[offset++] = (cc_uint8)(block >> 8);
		data[offset++] = (cc_uint8)(count & 0xFF);
		data[offset++] = (cc_uint8)(count >> 8);
		data[offset++] = (cc_uint8)(toolDurability[i] & 0xFF);
		data[offset++] = (cc_uint8)(toolDurability[i] >> 8);
		itemTypes++;
	}

	for (i = 0; i < INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		cc_uint16 block = (cc_uint16)Inventory.Table[i];
		data[offset++] = (cc_uint8)(block & 0xFF);
		data[offset++] = (cc_uint8)(block >> 8);
	}

	for (i = 0; i < SURVIVAL_ITEM_SLOTS; i++) {
		if (survivalItems[i].active && survivalItems[i].count > 0) droppedCount++;
	}
	data[offset++] = (cc_uint8)droppedCount;
	for (i = 0; i < SURVIVAL_ITEM_SLOTS; i++) {
		struct SurvivalItem* item = &survivalItems[i];
		cc_uint32 bits;
		if (!item->active || item->count <= 0) continue;
		data[offset++] = (cc_uint8)(item->block & 0xFF);
		data[offset++] = (cc_uint8)(item->block >> 8);
		data[offset++] = (cc_uint8)(item->count & 0xFF);
		data[offset++] = (cc_uint8)(item->count >> 8);
		data[offset++] = (cc_uint8)(item->durability & 0xFF);
		data[offset++] = (cc_uint8)(item->durability >> 8);
		Mem_Copy(&bits, &item->Base.Position.x, sizeof(bits));
		data[offset++] = (cc_uint8)(bits & 0xFF); data[offset++] = (cc_uint8)((bits >> 8) & 0xFF);
		data[offset++] = (cc_uint8)((bits >> 16) & 0xFF); data[offset++] = (cc_uint8)(bits >> 24);
		Mem_Copy(&bits, &item->Base.Position.y, sizeof(bits));
		data[offset++] = (cc_uint8)(bits & 0xFF); data[offset++] = (cc_uint8)((bits >> 8) & 0xFF);
		data[offset++] = (cc_uint8)((bits >> 16) & 0xFF); data[offset++] = (cc_uint8)(bits >> 24);
		Mem_Copy(&bits, &item->Base.Position.z, sizeof(bits));
		data[offset++] = (cc_uint8)(bits & 0xFF); data[offset++] = (cc_uint8)((bits >> 8) & 0xFF);
		data[offset++] = (cc_uint8)((bits >> 16) & 0xFF); data[offset++] = (cc_uint8)(bits >> 24);
	}

	Platform_EncodePath(&rawPath, &path);
	res = Stream_CreatePath(&stream, &rawPath);
	if (res) return res;
	res = Stream_Write(&stream, data, (cc_uint32)offset);
	if (res) {
		stream.Close(&stream);
		return res;
	}
	return stream.Close(&stream);
}

cc_result Survival_Load(const cc_string* mapPath) {
	struct Stream stream;
	cc_filepath rawPath;
	cc_string path;
	char pathBuffer[FILENAME_SIZE];
	cc_uint8 data[14 + BLOCK_COUNT * 6 + INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR * 2 + 1 + SURVIVAL_ITEM_SLOTS * 16];
	cc_uint32 payloadSize, droppedOffset;
	int i, offset, stackSlots = 0, droppedCount = 0;
	cc_result res;

	if (!Survival_IsActive()) return 0;
	if ((res = Survival_MakeSidecarPath(&path, pathBuffer, mapPath))) return res;
	Platform_EncodePath(&rawPath, &path);
	res = Stream_OpenPath(&stream, &rawPath);
	/* Older maps have no companion file; keep their fresh Survival state. */
	if (res) {
		Survival_OnNewMap();
		Survival_SpawnInitialAnimals();
		return 0;
	}

	res = Stream_Read(&stream, data, 12);
	if (res) {
		stream.Close(&stream);
		return res;
	}
	if (data[0] != 'C' || data[1] != 'C' || data[2] != 'S' || data[3] != 'V' ||
		(data[4] != 1 && data[4] != 2 && data[4] != 3 && data[4] != 4 && data[4] != 5) || data[9] > SURVIVAL_INVENTORY_SLOTS ||
		data[10] > HOTBAR_MAX_INDEX || data[11] >= INVENTORY_HOTBARS) {
		stream.Close(&stream);
		return ERR_INVALID_ARGUMENT;
	}

	payloadSize = (data[4] >= 2 ? 2U : 0U) + (cc_uint32)data[9] * (data[4] >= 3 ? 6U : 4U) +
		(cc_uint32)INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR * 2U;
	res = Stream_Read(&stream, data + 12, payloadSize);
	if (res) {
		stream.Close(&stream);
		return res;
	}
	droppedOffset = 12 + payloadSize;
	if (data[4] >= 4) {
		res = Stream_Read(&stream, data + droppedOffset, 1);
		if (res) {
			stream.Close(&stream);
			return res;
		}
		droppedCount = data[droppedOffset];
		if (droppedCount > SURVIVAL_ITEM_SLOTS) {
			stream.Close(&stream);
			return ERR_INVALID_ARGUMENT;
		}
		if (droppedCount > 0) {
			res = Stream_Read(&stream, data + droppedOffset + 1, (cc_uint32)droppedCount * (data[4] >= 5 ? 18U : 16U));
			if (res) {
				stream.Close(&stream);
				return res;
			}
		}
	}
	res = stream.Close(&stream);
	if (res) return res;

	Mem_Set(itemCounts, 0, sizeof(itemCounts));
	Mem_Set(toolDurability, 0, sizeof(toolDurability));
	health = min((int)data[5], SURVIVAL_MAX_HEALTH);
	food = min((int)data[6], SURVIVAL_MAX_FOOD);
	if (health == 0) health = SURVIVAL_MAX_HEALTH;
	airSeconds = min((float)data[7], SURVIVAL_MAX_AIR);
	saturation = min((float)data[8], (float)food);
	exhaustion = 0.0f;
	drowningTimer = hazardTimer = starvationTimer = regenerationTimer = deathTimer = 0.0f;
	fallActive = hasLastPosition = jumpWasRising = false;
	fallDistance = fallLastY = lastX = lastZ = 0.0f;
	Survival_CancelMining();

	offset = 12;
	if (data[4] >= 2) {
		cc_uint16 savedTicks = (cc_uint16)(data[12] | ((cc_uint16)data[13] << 8));
		timeOfDay = min((float)savedTicks, 2399.0f) * 0.5f;
		offset = 14;
	} else {
		timeOfDay = 0.0f;
	}
	dayCycleStep = -1;
	for (i = 0; i < data[9]; i++) {
		cc_uint16 block = (cc_uint16)(data[offset] | ((cc_uint16)data[offset + 1] << 8));
		cc_uint16 amount = (cc_uint16)(data[offset + 2] | ((cc_uint16)data[offset + 3] << 8));
		cc_uint16 durability = data[4] >= 3 ? (cc_uint16)(data[offset + 4] | ((cc_uint16)data[offset + 5] << 8)) : 0;
		offset += data[4] >= 3 ? 6 : 4;
		if (block == BLOCK_AIR || block >= BLOCK_COUNT || amount == 0) continue;
		itemCounts[block] = min((int)amount, SURVIVAL_INVENTORY_SLOTS * Survival_ItemMaxStack((BlockID)block));
		if (Survival_IsTool((BlockID)block)) {
			int maxDurability = Survival_ToolMaxDurability((BlockID)block);
			toolDurability[block] = data[4] >= 3 ? min((int)durability, maxDurability) : maxDurability;
			if (toolDurability[block] <= 0) {
				itemCounts[block] = 0;
				continue;
			}
		}
		stackSlots += (itemCounts[block] + Survival_ItemMaxStack((BlockID)block) - 1) / Survival_ItemMaxStack((BlockID)block);
	}
	if (stackSlots > SURVIVAL_INVENTORY_SLOTS) {
		Mem_Set(itemCounts, 0, sizeof(itemCounts));
		Survival_Reset();
		return ERR_INVALID_ARGUMENT;
	}

	for (i = 0; i < INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		cc_uint16 block = (cc_uint16)(data[offset] | ((cc_uint16)data[offset + 1] << 8));
		offset += 2;
		if (block >= BLOCK_COUNT || (block && itemCounts[block] <= 0)) block = BLOCK_AIR;
		Inventory.Table[i] = (BlockID)block;
	}
	Inventory.SelectedIndex = data[10];
	Inventory.Offset = data[11] * INVENTORY_BLOCKS_PER_HOTBAR;
	if (data[4] >= 4) {
		offset = (int)droppedOffset + 1;
		for (i = 0; i < droppedCount; i++) {
			cc_uint16 block = (cc_uint16)(data[offset] | ((cc_uint16)data[offset + 1] << 8));
			cc_uint16 amount = (cc_uint16)(data[offset + 2] | ((cc_uint16)data[offset + 3] << 8));
			cc_uint16 durability = data[4] >= 5 ? (cc_uint16)(data[offset + 4] | ((cc_uint16)data[offset + 5] << 8)) : 0;
			cc_uint32 bx, by, bz;
			Vec3 position;
			offset += data[4] >= 5 ? 6 : 4;
			bx = (cc_uint32)data[offset] | ((cc_uint32)data[offset + 1] << 8) |
				((cc_uint32)data[offset + 2] << 16) | ((cc_uint32)data[offset + 3] << 24);
			offset += 4;
			by = (cc_uint32)data[offset] | ((cc_uint32)data[offset + 1] << 8) |
				((cc_uint32)data[offset + 2] << 16) | ((cc_uint32)data[offset + 3] << 24);
			offset += 4;
			bz = (cc_uint32)data[offset] | ((cc_uint32)data[offset + 1] << 8) |
				((cc_uint32)data[offset + 2] << 16) | ((cc_uint32)data[offset + 3] << 24);
			offset += 4;
			Mem_Copy(&position.x, &bx, sizeof(bx));
			Mem_Copy(&position.y, &by, sizeof(by));
			Mem_Copy(&position.z, &bz, sizeof(bz));
			if (block == BLOCK_AIR || block >= BLOCK_COUNT || amount == 0 || amount > SURVIVAL_INVENTORY_STACK_SIZE) continue;
			/* Reject NaN and out-of-world positions from damaged sidecar files. */
			if (position.x != position.x || position.y != position.y || position.z != position.z) continue;
			if (position.x < 0.0f || position.x >= World.Width || position.y < 0.0f ||
				position.y >= World.Height || position.z < 0.0f || position.z >= World.Length) continue;
			SurvivalItem_SpawnEx((BlockID)block, amount, position, durability);
		}
	}
	Survival_RefreshInventoryMap();
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
	return 0;
}


int Survival_GetHealth(void) { return health; }
int Survival_GetFood(void)   { return food; }
int Survival_GetAir(void) {
	int value = (int)(airSeconds * 20.0f + 0.5f);
	return value < 0 ? 0 : value;
}
cc_bool Survival_IsDead(void) {
	return Server.IsSinglePlayer && health <= 0;
}

void Survival_Reset(void) {
	health = SURVIVAL_MAX_HEALTH;
	food = SURVIVAL_MAX_FOOD;
	saturation = 5.0f;
	exhaustion = 0.0f;
	airSeconds = SURVIVAL_MAX_AIR;
	drowningTimer = hazardTimer = starvationTimer = regenerationTimer = deathTimer = 0.0f;
	fallActive = hasLastPosition = jumpWasRising = false;
	fallDistance = fallLastY = lastX = lastZ = 0.0f;
}

void Survival_Damage(int amount) {
	if (!Survival_Active() || amount <= 0 || health <= 0) return;
	health -= amount;
	if (health < 0) health = 0;
	if (health == 0) {
		deathTimer = 3.0f;
		fallActive = false;
		Survival_DropInventory();
		Chat_AddRaw("&cYou died! Respawning...");
	}
}

void Survival_Heal(int amount) {
	if (!Survival_Active() || amount <= 0 || health <= 0) return;
	health += amount;
	if (health > SURVIVAL_MAX_HEALTH) health = SURVIVAL_MAX_HEALTH;
}

void Survival_AddFood(int foodPoints, float addedSaturation) {
	if (!Server.IsSinglePlayer || foodPoints < 0) return;
	food += foodPoints;
	if (food > SURVIVAL_MAX_FOOD) food = SURVIVAL_MAX_FOOD;
	saturation += addedSaturation;
	if (saturation > (float)food) saturation = (float)food;
}

void Survival_AddExhaustion(float amount) {
	if (!Server.IsSinglePlayer || amount <= 0.0f) return;
	exhaustion += amount;
}

static cc_bool Survival_HeadUnderwater(void) {
	Vec3 eye;
	BlockID block;
	if (!Entities.CurPlayer) return false;
	eye = Entity_GetEyePosition(&Entities.CurPlayer->Base);
	block = World_SafeGetBlock(Math_Floor(eye.x), Math_Floor(eye.y), Math_Floor(eye.z));
	return block == BLOCK_WATER || block == BLOCK_STILL_WATER;
}

static void Survival_Respawn(void) {
	struct LocalPlayer* player = Entities.CurPlayer;
	struct LocationUpdate update;
	if (!World.Loaded || !player) {
		Survival_Reset();
		return;
	}
	update.flags = LU_HAS_POS | LU_HAS_YAW | LU_HAS_PITCH;
	update.pos = player->Spawn;
	update.yaw = player->SpawnYaw;
	update.pitch = player->SpawnPitch;
	LocalPlayers_MoveToSpawn(&update);
	Vec3_Set(player->Base.Velocity, 0.0f, 0.0f, 0.0f);
	Survival_Reset();
}

static void Survival_ConsumeExhaustion(void) {
	while (exhaustion >= 4.0f) {
		exhaustion -= 4.0f;
		if (saturation > 0.0f) {
			saturation -= 1.0f;
			if (saturation < 0.0f) saturation = 0.0f;
		} else if (food > 0) {
			food--;
		}
	}
}

static void Survival_UpdateDayNight(float delta) {
	float phase, daylight, twilight;
	PackedCol sky, fog, sun;
	int step;

	timeOfDay += delta;
	while (timeOfDay >= 1200.0f) timeOfDay -= 1200.0f;
	step = (int)timeOfDay;
	if (step == dayCycleStep) return;
	dayCycleStep = step;

	phase = (timeOfDay / 1200.0f) * (2.0f * MATH_PI);
	daylight = (Math_CosF(phase) + 1.0f) * 0.5f;
	twilight = 1.0f - Math_AbsF(2.0f * daylight - 1.0f);

	sky = PackedCol_Lerp(PackedCol_Make(9, 15, 31, 255), PackedCol_Make(153, 204, 255, 255), daylight);
	fog = PackedCol_Lerp(PackedCol_Make(18, 22, 37, 255), PackedCol_Make(235, 242, 255, 255), daylight);
	sun = PackedCol_Lerp(PackedCol_Make(85, 98, 132, 255), PackedCol_Make(255, 255, 255, 255), daylight);

	sky = PackedCol_Lerp(sky, PackedCol_Make(238, 126, 73, 255), twilight * 0.32f);
	fog = PackedCol_Lerp(fog, PackedCol_Make(210, 126, 95, 255), twilight * 0.22f);
	sun = PackedCol_Lerp(sun, PackedCol_Make(255, 181, 120, 255), twilight * 0.35f);

	Env_SetSkyCol(sky);
	Env_SetFogCol(fog);
	Env_SetSunCol(sun);
}

static cc_bool Survival_Tick(struct ScheduledTask2* task) {
	const float dt = task->interval;
	if (!Survival_IsActive()) return true;
	Survival_UpdateDayNight(dt);
	Survival_CropTick(dt);
	if (Survival_IsNight()) {
		hostileSpawnTimer += dt;
		if (hostileSpawnTimer >= 12.0f && SurvivalMob_Count(true) < 5) {
			hostileSpawnTimer = 0.0f;
			Survival_SpawnNightMob();
		}
	} else {
		hostileSpawnTimer = 0.0f;
	}

	if (health <= 0) {
		deathTimer -= dt;
		if (deathTimer <= 0.0f) Survival_Respawn();
		return true;
	}

	Survival_ConsumeExhaustion();

	if (food >= 18 && health < SURVIVAL_MAX_HEALTH) {
		regenerationTimer += dt;
		if (regenerationTimer >= 4.0f) {
			regenerationTimer -= 4.0f;
			Survival_Heal(1);
			if (saturation > 0.0f) {
				saturation -= 1.0f;
				if (saturation < 0.0f) saturation = 0.0f;
			} else {
				exhaustion += 6.0f;
			}
		}
	} else {
		regenerationTimer = 0.0f;
	}

	if (food == 0) {
		starvationTimer += dt;
		if (starvationTimer >= 4.0f) {
			starvationTimer -= 4.0f;
			if (health > 1) Survival_Damage(1);
		}
	} else {
		starvationTimer = 0.0f;
	}

	if (Survival_HeadUnderwater()) {
		airSeconds -= dt;
		if (airSeconds < 0.0f) airSeconds = 0.0f;
		if (airSeconds <= 0.0f) {
			drowningTimer += dt;
			while (drowningTimer >= 1.0f && health > 0) {
				drowningTimer -= 1.0f;
				Survival_Damage(2);
			}
		} else {
			drowningTimer = 0.0f;
		}
	} else {
		airSeconds += dt * 4.0f;
		if (airSeconds > SURVIVAL_MAX_AIR) airSeconds = SURVIVAL_MAX_AIR;
		drowningTimer = 0.0f;
	}

	if (Entities.CurPlayer && Entity_TouchesAnyLava(&Entities.CurPlayer->Base)) {
		hazardTimer += dt;
		if (hazardTimer >= 1.0f) {
			hazardTimer -= 1.0f;
			Survival_Damage(4);
		}
	} else {
		hazardTimer = 0.0f;
	}
	return true;
}

void Survival_OnPlayerPhysics(struct LocalPlayer* player) {
	struct Entity* entity;
	float dx, dz, distance;
	int damage;

	if (!Survival_Active() || !player || player != Entities.CurPlayer || health <= 0) return;
	entity = &player->Base;

	if (player->Hacks.Floating || player->Hacks.Noclip) {
		fallActive = false;
		fallDistance = 0.0f;
		hasLastPosition = false;
		jumpWasRising = false;
		return;
	}

	/* Water cancels fall damage, as in Minecraft. */
	if (Entity_TouchesAnyWater(entity)) {
		fallActive = false;
		fallDistance = 0.0f;
	} else if (!entity->OnGround && entity->Velocity.y < 0.0f) {
		if (!fallActive) {
			fallActive = true;
			fallDistance = 0.0f;
			fallLastY = entity->Position.y;
		} else if (fallLastY > entity->Position.y) {
			fallDistance += fallLastY - entity->Position.y;
		}
		fallLastY = entity->Position.y;
	} else if (entity->OnGround && fallActive) {
		if (fallLastY > entity->Position.y) fallDistance += fallLastY - entity->Position.y;
		damage = Math_Floor(fallDistance - 3.0f);
		if (damage > 0) Survival_Damage(damage);
		fallActive = false;
		fallDistance = 0.0f;
	}

	if (hasLastPosition && entity->OnGround) {
		dx = entity->Position.x - lastX;
		dz = entity->Position.z - lastZ;
		distance = Math_SqrtF(dx * dx + dz * dz);
		if (distance > 0.02f && distance < 1.5f) exhaustion += distance * 0.01f;
	}
	lastX = entity->Position.x;
	lastZ = entity->Position.z;
	hasLastPosition = true;

	if (entity->Velocity.y > 0.2f) {
		if (!jumpWasRising) exhaustion += 0.05f;
		jumpWasRising = true;
	} else if (entity->Velocity.y <= 0.0f) {
		jumpWasRising = false;
	}
}


static cc_bool Survival_NearBlock(BlockID block) {
	Vec3 pos;
	int x, y, z, dx, dy, dz;
	if (!Entities.CurPlayer) return false;
	pos = Entities.CurPlayer->Base.Position;
	x = Math_Floor(pos.x); y = Math_Floor(pos.y); z = Math_Floor(pos.z);
	for (dy = -1; dy <= 1; dy++) {
		for (dz = -3; dz <= 3; dz++) {
			for (dx = -3; dx <= 3; dx++) {
				if (World_SafeGetBlock(x + dx, y + dy, z + dz) == block) return true;
			}
		}
	}
	return false;
}

static cc_bool Survival_CraftRecipe(BlockID inputA, int amountA, BlockID inputB, int amountB,
	BlockID output, int amountOut, BlockID station) {
	int added;
	if (station != BLOCK_AIR && !Survival_NearBlock(station)) {
		Chat_AddRaw("&eYou need the correct crafting station nearby.");
		return false;
	}
	if (Survival_GetItemCount(inputA) < amountA ||
		(amountB > 0 && Survival_GetItemCount(inputB) < amountB)) {
		Chat_AddRaw("&eYou don't have the required materials.");
		return false;
	}
	/* Free consumed slots before checking output capacity, so a full inventory
	   can still craft a recipe that replaces its ingredients with an output. */
	if (Survival_InventoryRemove(inputA, amountA) != amountA) return false;
	if (amountB > 0 && Survival_InventoryRemove(inputB, amountB) != amountB) {
		Survival_InventoryAdd(inputA, amountA);
		return false;
	}
	added = Survival_InventoryAdd(output, amountOut);
	if (added != amountOut) {
		if (added > 0) Survival_InventoryRemove(output, added);
		Survival_InventoryAdd(inputA, amountA);
		if (amountB > 0) Survival_InventoryAdd(inputB, amountB);
		Chat_AddRaw("&eYour inventory is full.");
		return false;
	}
	Chat_AddRaw("&aCrafted successfully.");
	return true;
}

static void SurvivalCraftCommand_Execute(const cc_string* args, int argsCount) {
	const cc_string* recipe;
	if (!Server.IsSinglePlayer || Game_ClassicMode) return;
	if (!argsCount || String_CaselessEqualsConst(&args[0], "help")) {
		Chat_AddRaw("&a/craft planks &7- 1 log to 4 planks");
		Chat_AddRaw("&a/craft sticks &7- 2 planks to 4 sticks");
		Chat_AddRaw("&a/craft table &7- 4 planks to a crafting table");
		Chat_AddRaw("&a/craft furnace &7- 8 cobblestone to a furnace");
		Chat_AddRaw("&a/craft torch &7- 1 coal + 1 stick to 4 torches");
		Chat_AddRaw("&a/craft wooden_pickaxe|stone_pickaxe|iron_pickaxe");
		Chat_AddRaw("&a/craft wooden_axe|stone_axe|iron_axe");
		Chat_AddRaw("&a/craft wooden_shovel|stone_shovel|iron_shovel");
		Chat_AddRaw("&a/craft wooden_sword|stone_sword|iron_sword");
		Chat_AddRaw("&a/craft wooden_hoe|stone_hoe|iron_hoe");
		Chat_AddRaw("&a/craft bread &7- 3 wheat");
		Chat_AddRaw("&aUse hoes on dirt/grass, then plant seeds on farmland.");
		Chat_AddRaw("&a/craft smelt_iron|smelt_gold|cook_pork|cook_beef|cook_chicken");
		return;
	}
	recipe = &args[0];
	if (String_CaselessEqualsConst(recipe, "planks")) {
		Survival_CraftRecipe(BLOCK_LOG, 1, BLOCK_AIR, 0, BLOCK_WOOD, 4, BLOCK_AIR);
	} else if (String_CaselessEqualsConst(recipe, "sticks")) {
		Survival_CraftRecipe(BLOCK_WOOD, 2, BLOCK_AIR, 0, BLOCK_ITEM_STICK, 4, BLOCK_AIR);
	} else if (String_CaselessEqualsConst(recipe, "table")) {
		Survival_CraftRecipe(BLOCK_WOOD, 4, BLOCK_AIR, 0, BLOCK_ITEM_CRAFTING_TABLE, 1, BLOCK_AIR);
	} else if (String_CaselessEqualsConst(recipe, "furnace")) {
		Survival_CraftRecipe(BLOCK_COBBLE, 8, BLOCK_AIR, 0, BLOCK_ITEM_FURNACE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "torch")) {
		Survival_CraftRecipe(BLOCK_ITEM_COAL, 1, BLOCK_ITEM_STICK, 1, BLOCK_ITEM_TORCH, 4, BLOCK_AIR);
	} else if (String_CaselessEqualsConst(recipe, "wooden_pickaxe")) {
		Survival_CraftRecipe(BLOCK_WOOD, 3, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_WOOD_PICKAXE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "stone_pickaxe")) {
		Survival_CraftRecipe(BLOCK_COBBLE, 3, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_STONE_PICKAXE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "wooden_axe")) {
		Survival_CraftRecipe(BLOCK_WOOD, 3, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_WOOD_AXE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "stone_axe")) {
		Survival_CraftRecipe(BLOCK_COBBLE, 3, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_STONE_AXE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "wooden_shovel")) {
		Survival_CraftRecipe(BLOCK_WOOD, 1, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_WOOD_SHOVEL, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "stone_shovel")) {
		Survival_CraftRecipe(BLOCK_COBBLE, 1, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_STONE_SHOVEL, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "bread")) {
		Survival_CraftRecipe(BLOCK_ITEM_WHEAT, 3, BLOCK_AIR, 0, BLOCK_ITEM_BREAD, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "wooden_hoe")) {
		Survival_CraftRecipe(BLOCK_WOOD, 2, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_WOOD_HOE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "stone_hoe")) {
		Survival_CraftRecipe(BLOCK_COBBLE, 2, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_STONE_HOE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "iron_hoe")) {
		Survival_CraftRecipe(BLOCK_ITEM_IRON_INGOT, 2, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_IRON_HOE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "iron_pickaxe")) {
		Survival_CraftRecipe(BLOCK_ITEM_IRON_INGOT, 3, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_IRON_PICKAXE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "iron_axe")) {
		Survival_CraftRecipe(BLOCK_ITEM_IRON_INGOT, 3, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_IRON_AXE, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "iron_shovel")) {
		Survival_CraftRecipe(BLOCK_ITEM_IRON_INGOT, 1, BLOCK_ITEM_STICK, 2, BLOCK_ITEM_IRON_SHOVEL, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "wooden_sword")) {
		Survival_CraftRecipe(BLOCK_WOOD, 2, BLOCK_ITEM_STICK, 1, BLOCK_ITEM_WOOD_SWORD, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "stone_sword")) {
		Survival_CraftRecipe(BLOCK_COBBLE, 2, BLOCK_ITEM_STICK, 1, BLOCK_ITEM_STONE_SWORD, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "iron_sword")) {
		Survival_CraftRecipe(BLOCK_ITEM_IRON_INGOT, 2, BLOCK_ITEM_STICK, 1, BLOCK_ITEM_IRON_SWORD, 1, BLOCK_ITEM_CRAFTING_TABLE);
	} else if (String_CaselessEqualsConst(recipe, "smelt_iron")) {
		Survival_CraftRecipe(BLOCK_ITEM_IRON_ORE, 1, BLOCK_ITEM_COAL, 1, BLOCK_ITEM_IRON_INGOT, 1, BLOCK_ITEM_FURNACE);
	} else if (String_CaselessEqualsConst(recipe, "smelt_gold")) {
		Survival_CraftRecipe(BLOCK_ITEM_GOLD_ORE, 1, BLOCK_ITEM_COAL, 1, BLOCK_ITEM_GOLD_INGOT, 1, BLOCK_ITEM_FURNACE);
	} else if (String_CaselessEqualsConst(recipe, "cook_pork")) {
		Survival_CraftRecipe(BLOCK_ITEM_RAW_PORK, 1, BLOCK_ITEM_COAL, 1, BLOCK_ITEM_COOKED_PORK, 1, BLOCK_ITEM_FURNACE);
	} else if (String_CaselessEqualsConst(recipe, "cook_beef")) {
		Survival_CraftRecipe(BLOCK_ITEM_RAW_BEEF, 1, BLOCK_ITEM_COAL, 1, BLOCK_ITEM_COOKED_BEEF, 1, BLOCK_ITEM_FURNACE);
	} else if (String_CaselessEqualsConst(recipe, "cook_chicken")) {
		Survival_CraftRecipe(BLOCK_ITEM_RAW_CHICKEN, 1, BLOCK_ITEM_COAL, 1, BLOCK_ITEM_COOKED_CHICKEN, 1, BLOCK_ITEM_FURNACE);
	} else {
		Chat_AddRaw("&eUnknown recipe. Use /client craft help.");
	}
}

static struct ChatCommand SurvivalCraftCommand = {
	"craft", SurvivalCraftCommand_Execute,
	COMMAND_FLAG_SINGLEPLAYER_ONLY,
	{
		"&a/client craft [recipe]",
		"&eCraft Survival materials, blocks, food and tools.",
	}
};


#define SURVIVAL_MOB_SLOTS 16
#define SURVIVAL_MOB_PIG 0
#define SURVIVAL_MOB_CHICKEN 1
#define SURVIVAL_MOB_SHEEP 2
#define SURVIVAL_MOB_ZOMBIE 3
#define SURVIVAL_MOB_SKELETON 4
#define SURVIVAL_MOB_CREEPER 5
#define SURVIVAL_MOB_SPIDER 6

struct SurvivalMob {
	struct Entity Base;
	struct HacksComp Hacks;
	struct PhysicsComp Physics;
	struct CollisionsComp Collisions;
	struct SoundsComp Sounds;
	int entityID;
	int type;
	int health;
	int maxHealth;
	float targetX, targetZ;
	float retargetTimer;
	float attackTimer;
	float hurtTimer;
	float burnTimer;
	cc_bool active;
};

static struct SurvivalMob survivalMobs[SURVIVAL_MOB_SLOTS];
static void SurvivalMob_Tick(struct Entity* e, float delta);

static cc_bool SurvivalMob_IsHostile(const struct SurvivalMob* mob) {
	return mob->type >= SURVIVAL_MOB_ZOMBIE;
}

static PackedCol SurvivalMob_GetColor(struct Entity* e) {
	(void)e;
	return PACKEDCOL_WHITE;
}

static void SurvivalMob_Despawn(struct Entity* e) {
	struct SurvivalMob* mob = (struct SurvivalMob*)e;
	if (e->Flags & ENTITY_FLAG_HAS_MODELVB) Gfx_DeleteScratchVb(&e->ModelVB);
	mob->active = false;
}

static void SurvivalMob_SetLocation(struct Entity* e, struct LocationUpdate* update) {
	if (update->flags & LU_HAS_POS) {
		e->Position = update->pos;
		e->prev.pos = update->pos;
		e->next.pos = update->pos;
	}
	if (update->flags & LU_HAS_YAW) e->Yaw = update->yaw;
	if (update->flags & LU_HAS_PITCH) e->Pitch = update->pitch;
}

static void SurvivalMob_RenderModel(struct Entity* e, float delta, float t) {
	(void)delta;
	Vec3_Lerp(&e->Position, &e->prev.pos, &e->next.pos, t);
	AnimatedComp_GetCurrent(e, t);
	e->ShouldRender = Model_ShouldRender(e);
	if (e->ShouldRender) Model_Render(e->Model, e);
}

static cc_bool SurvivalMob_ShouldRenderName(struct Entity* e) {
	(void)e;
	return false;
}

static const struct EntityVTABLE survivalMob_VTABLE = {
	SurvivalMob_Tick, SurvivalMob_Despawn, SurvivalMob_SetLocation, SurvivalMob_GetColor,
	SurvivalMob_RenderModel, SurvivalMob_ShouldRenderName
};

static struct SurvivalMob* SurvivalMob_FindFree(void) {
	int i;
	for (i = 0; i < SURVIVAL_MOB_SLOTS; i++) {
		if (!survivalMobs[i].active && !Entities.List[i]) return &survivalMobs[i];
	}
	return NULL;
}

static int SurvivalMob_FindID(struct SurvivalMob* mob) {
	int i;
	for (i = 0; i < SURVIVAL_MOB_SLOTS; i++) {
		if (&survivalMobs[i] == mob) return i;
	}
	return -1;
}

static int SurvivalMob_Count(cc_bool hostileOnly) {
	int i, count = 0;
	for (i = 0; i < SURVIVAL_MOB_SLOTS; i++) {
		if (!survivalMobs[i].active) continue;
		if (hostileOnly && !SurvivalMob_IsHostile(&survivalMobs[i])) continue;
		count++;
	}
	return count;
}

static struct SurvivalMob* SurvivalMob_Spawn(int type, float x, float z) {
	static const char* const modelNames[] = {
		"pig", "chicken", "sheep", "zombie", "skeleton", "creeper", "spider"
	};
	struct SurvivalMob* mob = SurvivalMob_FindFree();
	cc_string modelName;
	int id;

	if (!mob || type < SURVIVAL_MOB_PIG || type > SURVIVAL_MOB_SPIDER) return NULL;
	id = SurvivalMob_FindID(mob);
	if (id < 0) return NULL;

	Mem_Set(mob, 0, sizeof(*mob));
	mob->entityID = id;
	mob->type = type;
	mob->health = mob->maxHealth = SurvivalMob_IsHostile(mob) ? 20 : 10;
	mob->active = true;

	Entity_Init(&mob->Base);
	modelName = String_FromReadonly(modelNames[type]);
	Entity_SetModel(&mob->Base, &modelName);
	mob->Base.VTABLE = &survivalMob_VTABLE;

	HacksComp_Init(&mob->Hacks);
	mob->Hacks.CanBePushed = false;
	mob->Hacks.Floating = false;
	PhysicsComp_Init(&mob->Physics, &mob->Base);
	mob->Physics.Hacks = &mob->Hacks;
	mob->Physics.Collisions = &mob->Collisions;
	mob->Collisions.Entity = &mob->Base;
	mob->Collisions.StepSize = 0.5f;
	SoundsComp_Init(&mob->Sounds, &mob->Base);
	AnimatedComp_Init(&mob->Base.Anim);

	mob->Base.Position = Respawn_FindSpawnPosition(x, z, mob->Base.Size);
	mob->Base.prev.pos = mob->Base.Position;
	mob->Base.next.pos = mob->Base.Position;
	mob->Base.Yaw = (float)Random_Next(&survivalRng, 360);
	mob->targetX = mob->Base.Position.x;
	mob->targetZ = mob->Base.Position.z;
	mob->retargetTimer = 0.1f;
	Entities.List[id] = &mob->Base;
	return mob;
}

static void SurvivalMob_ClearAll(void) {
	int i;
	for (i = 0; i < SURVIVAL_MOB_SLOTS; i++) {
		if (survivalMobs[i].active) Entities_Remove(survivalMobs[i].entityID);
	}
	hostileSpawnTimer = 0.0f;
}

static void SurvivalMob_AddDrop(struct SurvivalMob* mob, BlockID block, int amount) {
	Vec3 position;
	if (!mob || amount <= 0) return;
	position = mob->Base.Position;
	position.y += 0.25f;
	SurvivalItem_Spawn(block, amount, position);
}

static void SurvivalMob_Die(struct SurvivalMob* mob) {
	if (!mob || !mob->active) return;

	switch (mob->type) {
	case SURVIVAL_MOB_PIG:
		SurvivalMob_AddDrop(mob, BLOCK_ITEM_RAW_PORK, 1 + Random_Next(&survivalRng, 3));
		break;
	case SURVIVAL_MOB_CHICKEN:
		SurvivalMob_AddDrop(mob, BLOCK_ITEM_RAW_CHICKEN, 1 + Random_Next(&survivalRng, 2));
		break;
	case SURVIVAL_MOB_SHEEP:
		SurvivalMob_AddDrop(mob, BLOCK_WHITE, 1 + Random_Next(&survivalRng, 2));
		break;
	case SURVIVAL_MOB_ZOMBIE:
		SurvivalMob_AddDrop(mob, BLOCK_ITEM_ROTTEN_FLESH, 1 + Random_Next(&survivalRng, 2));
		break;
	case SURVIVAL_MOB_SKELETON:
		SurvivalMob_AddDrop(mob, BLOCK_ITEM_BONE, 1 + Random_Next(&survivalRng, 2));
		SurvivalMob_AddDrop(mob, BLOCK_ITEM_ARROW, 1 + Random_Next(&survivalRng, 2));
		break;
	case SURVIVAL_MOB_CREEPER:
		SurvivalMob_AddDrop(mob, BLOCK_ITEM_GUNPOWDER, 1 + Random_Next(&survivalRng, 2));
		break;
	case SURVIVAL_MOB_SPIDER:
		SurvivalMob_AddDrop(mob, BLOCK_ITEM_STRING, 1 + Random_Next(&survivalRng, 2));
		break;
	default:
		break;
	}
	Entities_Remove(mob->entityID);
}

static void SurvivalMob_SetWanderTarget(struct SurvivalMob* mob) {
	float angle = (float)Random_Next(&survivalRng, 6283) / 1000.0f;
	float range = 2.0f + (float)Random_Next(&survivalRng, 500) / 100.0f;
	mob->targetX = mob->Base.Position.x + Math_CosF(angle) * range;
	mob->targetZ = mob->Base.Position.z + Math_SinF(angle) * range;
	if (mob->targetX < 2.0f) mob->targetX = 2.0f;
	if (mob->targetZ < 2.0f) mob->targetZ = 2.0f;
	if (mob->targetX > World.Width - 2.0f) mob->targetX = World.Width - 2.0f;
	if (mob->targetZ > World.Length - 2.0f) mob->targetZ = World.Length - 2.0f;
	mob->retargetTimer = 2.0f + (float)Random_Next(&survivalRng, 400) / 100.0f;
}

static cc_bool SurvivalMob_ExposedToSky(struct Entity* e) {
	int x = Math_Floor(e->Position.x);
	int y = Math_Floor(e->Position.y + e->Size.y - 0.1f);
	int z = Math_Floor(e->Position.z);
	if (!World_Contains(x, 0, z)) return false;
	if (y < 0) y = 0;
	for (; y < World.Height; y++) {
		BlockID block = World_SafeGetBlock(x, y, z);
		if (Blocks.BlocksLight[block]) return false;
	}
	return true;
}

static void SurvivalMob_Tick(struct Entity* e, float delta) {
	struct SurvivalMob* mob = (struct SurvivalMob*)e;
	struct LocalPlayer* player = Entities.CurPlayer;
	Vec3 oldPos = e->Position;
	Vec3 movement = { 0.0f, 0.0f, 0.0f };
	cc_bool wasOnGround = e->OnGround;
	float dx, dz, distance, speed;
	cc_bool hostile = SurvivalMob_IsHostile(mob);

	if (!mob->active || !Survival_IsActive() || !World.Loaded || !player) return;
	mob->retargetTimer -= delta;
	mob->attackTimer -= delta;
	mob->hurtTimer -= delta;

	/* Zombies and skeletons burn in direct daylight, but are safe under a roof. */
	if ((mob->type == SURVIVAL_MOB_ZOMBIE || mob->type == SURVIVAL_MOB_SKELETON) &&
		!Survival_IsNight() && SurvivalMob_ExposedToSky(e)) {
		mob->burnTimer += delta;
		if (mob->burnTimer >= 1.0f) {
			mob->burnTimer -= 1.0f;
			mob->health -= 2;
			if (mob->health <= 0) {
				SurvivalMob_Die(mob);
				return;
			}
		}
	} else {
		mob->burnTimer = 0.0f;
	}

	if (hostile) {
		dx = player->Base.Position.x - e->Position.x;
		dz = player->Base.Position.z - e->Position.z;
		distance = Math_SqrtF(dx * dx + dz * dz);
		if (distance < 1.35f) {
			if (mob->attackTimer <= 0.0f && !Survival_IsDead()) {
				Survival_Damage(mob->type == SURVIVAL_MOB_CREEPER ? 4 : 2);
				mob->attackTimer = 1.25f;
			}
			dx = dz = 0.0f;
		} else if (distance < 20.0f && distance > 0.001f) {
			dx /= distance;
			dz /= distance;
		} else {
			if (mob->retargetTimer <= 0.0f) SurvivalMob_SetWanderTarget(mob);
			dx = mob->targetX - e->Position.x;
			dz = mob->targetZ - e->Position.z;
			distance = Math_SqrtF(dx * dx + dz * dz);
			if (distance > 0.3f) { dx /= distance; dz /= distance; }
			else { dx = dz = 0.0f; }
		}
		speed = 0.65f;
	} else {
		if (mob->retargetTimer <= 0.0f) SurvivalMob_SetWanderTarget(mob);
		dx = mob->targetX - e->Position.x;
		dz = mob->targetZ - e->Position.z;
		distance = Math_SqrtF(dx * dx + dz * dz);
		if (distance > 0.35f) { dx /= distance; dz /= distance; }
		else { dx = dz = 0.0f; }
		speed = 0.28f;
	}

	if (dx != 0.0f || dz != 0.0f) {
		movement.x = dx * speed;
		movement.z = dz * speed;
		e->Yaw = Math_Atan2f(dz, dx) * MATH_RAD2DEG;
	}
	PhysicsComp_UpdateVelocityState(&mob->Physics);
	PhysicsComp_PhysicsTick(&mob->Physics, movement);
	e->prev.pos = oldPos;
	e->next.pos = e->Position;
	AnimatedComp_Update(e, oldPos, e->Position, delta);
	SoundsComp_Tick(&mob->Sounds, wasOnGround);

	if (e->Position.y < 0.0f) SurvivalMob_Die(mob);
}

static cc_bool Survival_IsNight(void) {
	return timeOfDay >= 540.0f && timeOfDay <= 1120.0f;
}

static void Survival_SpawnInitialAnimals(void) {
	Vec3 centre;
	if (!Entities.CurPlayer) return;
	centre = Entities.CurPlayer->Base.Position;
	SurvivalMob_Spawn(SURVIVAL_MOB_PIG, centre.x + 3.0f, centre.z + 2.0f);
	SurvivalMob_Spawn(SURVIVAL_MOB_PIG, centre.x - 3.0f, centre.z - 2.0f);
	SurvivalMob_Spawn(SURVIVAL_MOB_CHICKEN, centre.x + 5.0f, centre.z - 3.0f);
	SurvivalMob_Spawn(SURVIVAL_MOB_CHICKEN, centre.x - 5.0f, centre.z + 3.0f);
	SurvivalMob_Spawn(SURVIVAL_MOB_SHEEP, centre.x + 2.0f, centre.z - 5.0f);
}

static void Survival_SpawnNightMob(void) {
	Vec3 centre;
	int type;
	if (!Entities.CurPlayer) return;
	centre = Entities.CurPlayer->Base.Position;
	type = SURVIVAL_MOB_ZOMBIE + Random_Next(&survivalRng, 4);
	SurvivalMob_Spawn(type,
		centre.x + (float)(Random_Next(&survivalRng, 2) ? 1 : -1) * (8.0f + Random_Next(&survivalRng, 6)),
		centre.z + (float)(Random_Next(&survivalRng, 2) ? 1 : -1) * (8.0f + Random_Next(&survivalRng, 6)));
}

cc_bool Survival_AttackMob(void) {
	int i, damage;
	struct SurvivalMob* mob = NULL;
	struct Entity* target;
	float dx, dy, dz, distance, closestDistance = 4.5f;
	Vec3 eye;
	float blockDX, blockDY, blockDZ, blockDistance;

	if (!Survival_IsActive() || !Entities.CurPlayer) return false;

	/* Search the mob pool directly; dropped-item entities share the general entity list. */
	for (i = 0; i < SURVIVAL_MOB_SLOTS; i++) {
		if (!survivalMobs[i].active) continue;
		target = &survivalMobs[i].Base;
		dx = target->Position.x - Entities.CurPlayer->Base.Position.x;
		dy = target->Position.y - Entities.CurPlayer->Base.Position.y;
		dz = target->Position.z - Entities.CurPlayer->Base.Position.z;
		distance = Math_SqrtF(dx * dx + dy * dy + dz * dz);
		if (distance < closestDistance) {
			closestDistance = distance;
			mob = &survivalMobs[i];
		}
	}
	if (!mob) return false;
	target = &mob->Base;
	distance = closestDistance;
	/* Do not hit a mob through the nearer block selected by the normal block ray. */
	if (Game_SelectedPos.valid) {
		eye = Entity_GetEyePosition(&Entities.CurPlayer->Base);
		blockDX = (float)Game_SelectedPos.pos.x + 0.5f - eye.x;
		blockDY = (float)Game_SelectedPos.pos.y + 0.5f - eye.y;
		blockDZ = (float)Game_SelectedPos.pos.z + 0.5f - eye.z;
		blockDistance = Math_SqrtF(blockDX * blockDX + blockDY * blockDY + blockDZ * blockDZ);
		if (blockDistance + 0.2f < distance) return false;
	}
	if (mob->hurtTimer > 0.0f) return true;

	{
		BlockID held = Inventory_SelectedBlock;
		damage = 2;
		if (held == BLOCK_ITEM_WOOD_AXE || held == BLOCK_ITEM_STONE_AXE || held == BLOCK_ITEM_IRON_AXE) damage = 4;
		if (held == BLOCK_ITEM_WOOD_SWORD) damage = 4;
		if (held == BLOCK_ITEM_STONE_SWORD) damage = 5;
		if (held == BLOCK_ITEM_IRON_SWORD) damage = 6;
		mob->health -= damage;
		if (Survival_IsTool(held)) {
			toolDurability[held]--;
			if (toolDurability[held] <= 0) {
				Chat_AddRaw("&cYour weapon broke.");
				Survival_InventoryRemove(held, 1);
			}
		}
	}
	mob->hurtTimer = 0.45f;
	Survival_CancelMining();
	if (mob->health <= 0) SurvivalMob_Die(mob);
	return true;
}

static void Survival_OnNewMapLoaded(void) {
	if (!Survival_IsActive()) return;
	Survival_CropRebuild();
	Survival_RefreshInventoryMap();
	SurvivalMob_ClearAll();
	Survival_SpawnInitialAnimals();
	Chat_AddRaw("&aSurvival Mode enabled. Use /client craft help for recipes.");
}

static void Survival_DrawPixelMask(int x, int y, const cc_uint8* rows, int width, int height, PackedCol col, int maxColumns) {
	int row, column;
	if (maxColumns < 0 || maxColumns > width) maxColumns = width;
	for (row = 0; row < height; row++) {
		for (column = 0; column < maxColumns; column++) {
			if (rows[row] & (1 << (width - 1 - column))) {
				Gfx_Draw2DFlat(x + column * 2, y + row * 2, 2, 2, col);
			}
		}
	}
}

static void Survival_DrawHeart(int x, int y, int points) {
	static const cc_uint8 outer[6] = { 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18 };
	static const cc_uint8 inner[4] = { 0x33, 0x3F, 0x3F, 0x1E };
	PackedCol border = PackedCol_Make(18, 12, 16, 255);
	PackedCol empty  = PackedCol_Make(74, 49, 54, 255);
	PackedCol full   = PackedCol_Make(222, 45, 55, 255);
	Survival_DrawPixelMask(x, y, outer, 8, 6, border, 8);
	Survival_DrawPixelMask(x + 2, y + 2, inner, 6, 4, points > 0 ? full : empty, points == 1 ? 3 : 6);
}

static void Survival_DrawFoodIcon(int x, int y, int points) {
	static const cc_uint8 outer[8] = { 0x18, 0x3C, 0x7E, 0x7E, 0x7E, 0x3C, 0x24, 0x42 };
	static const cc_uint8 inner[6] = { 0x0C, 0x1E, 0x3F, 0x3F, 0x1E, 0x0C };
	PackedCol border = PackedCol_Make(25, 18, 10, 255);
	PackedCol empty  = PackedCol_Make(79, 61, 42, 255);
	PackedCol full   = PackedCol_Make(208, 151, 72, 255);
	Survival_DrawPixelMask(x, y, outer, 8, 8, border, 8);
	Survival_DrawPixelMask(x + 2, y + 2, inner, 6, 6, points > 0 ? full : empty, points == 1 ? 3 : 6);
}

static void Survival_DrawAirBubble(int x, int y) {
	static const cc_uint8 bubble[4] = { 0x6, 0xF, 0xF, 0x6 };
	Survival_DrawPixelMask(x, y, bubble, 4, 4, PackedCol_Make(45, 120, 200, 255), 4);
}

static void Survival_RenderHUD(float delta) {
	int i, y, foodX;
	(void)delta;
	if (!Survival_Active() || Game_HideGui) return;
	y = Game.Height - 54;
	if (y < 4) y = 4;

	/* Minecraft-style 10-heart health row and 10-piece hunger row. */
	for (i = 0; i < 10; i++) {
		Survival_DrawHeart(12 + i * 18, y, max(0, min(2, health - i * 2)));
	}
	foodX = Game.Width - 12 - 10 * 18;
	for (i = 0; i < 10; i++) {
		Survival_DrawFoodIcon(foodX + i * 18, y, max(0, min(2, food - i * 2)));
	}

	if (airSeconds < SURVIVAL_MAX_AIR) {
		int bubbles = (int)((airSeconds / SURVIVAL_MAX_AIR) * 10.0f + 0.5f);
		int airX = (Game.Width - 10 * 12) / 2;
		for (i = 0; i < bubbles; i++) Survival_DrawAirBubble(airX + i * 12, y - 14);
	}
	if (miningActive) {
		int barX = (Game.Width - 66) / 2;
		int barY = Game.Height / 2 + 18;
		int barWidth = (int)(64.0f * Survival_GetMiningProgress());
		Gfx_Draw2DFlat(barX - 1, barY - 1, 66, 7, PackedCol_Make(0, 0, 0, 200));
		if (barWidth > 0) Gfx_Draw2DFlat(barX, barY, barWidth, 5, PackedCol_Make(235, 180, 65, 255));
	}
}

static void Survival_ComponentReset(void) {
	/* Remove local Survival entities before switching maps or game modes. */
	SurvivalMob_ClearAll();
	SurvivalItem_ClearAll();
	Survival_Reset();
}

static void Survival_Init(void) {
	Random_Seed(&survivalRng, (int)Stopwatch_Measure());
	Survival_Reset();
	Commands_Register(&SurvivalCraftCommand);
	timeOfDay = 0.0f;
	dayCycleStep = -1;
	Game.Draw2DHooks[1] = Survival_RenderHUD;
	survivalTask.accumulator = 0.0f;
	survivalTask.interval = SURVIVAL_TICK;
	survivalTask.callback = Survival_Tick;
	survivalTask.next = NULL;
	ScheduledTask2_Add(&survivalTask);
}

static void Survival_Free(void) {
	SurvivalMob_ClearAll();
	SurvivalItem_ClearAll();
	Commands_Unregister(&SurvivalCraftCommand);
	if (Game.Draw2DHooks[1] == Survival_RenderHUD) Game.Draw2DHooks[1] = NULL;
}

struct IGameComponent Survival_Component = {
	Survival_Init,
	Survival_Free,
	Survival_ComponentReset,
	Survival_OnNewMap,
	Survival_OnNewMapLoaded,
	NULL
};
