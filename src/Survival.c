#include "Survival.h"
#include "BlockID.h"
#include "Chat.h"
#include "Entity.h"
#include "ExtMath.h"
#include "Game.h"
#include "Inventory.h"
#include "Event.h"
#include "Graphics.h"
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
static int itemCounts[BLOCK_COUNT];

cc_bool Survival_IsActive(void) {
	return Server.IsSinglePlayer && World.Loaded;
}
#define Survival_Active Survival_IsActive

static int Survival_UsedInventoryStacks(void) {
	int i, slots = 0;
	for (i = 1; i < BLOCK_COUNT; i++) slots += (itemCounts[i] + 63) / 64;
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
	int used, freeSlots, room, capacity, i;
	if (!Survival_IsActive() || block <= BLOCK_AIR || block >= BLOCK_COUNT || amount <= 0) return 0;
	used = Survival_UsedInventoryStacks();
	freeSlots = SURVIVAL_INVENTORY_SLOTS - used;
	if (freeSlots < 0) freeSlots = 0;
	room = itemCounts[block] % 64;
	room = room ? 64 - room : 0;
	capacity = room + freeSlots * 64;
	if (amount > capacity) amount = capacity;
	if (amount <= 0) return 0;

	itemCounts[block] += amount;
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

static BlockID Survival_GetBlockDrop(BlockID block) {
	if (block == BLOCK_GRASS) return BLOCK_DIRT;
	if (block == BLOCK_STONE) return BLOCK_COBBLE;
	if (block == BLOCK_LEAVES) {
		return ((int)(Game.Time * 10.0) % 5) == 0 ? BLOCK_SAPLING : BLOCK_AIR;
	}
	if (block == BLOCK_AIR || block == BLOCK_WATER || block == BLOCK_STILL_WATER ||
		block == BLOCK_LAVA || block == BLOCK_STILL_LAVA || block == BLOCK_BEDROCK) return BLOCK_AIR;
	return block;
}

void Survival_OnBlockBroken(BlockID block) {
	BlockID drop;
	if (!Survival_IsActive()) return;
	drop = Survival_GetBlockDrop(block);
	if (drop != BLOCK_AIR) Survival_InventoryAdd(drop, 1);
}

static void Survival_OnNewMap(void) {
	int i;
	if (!Server.IsSinglePlayer) return;
	Survival_Reset();
	Mem_Set(itemCounts, 0, sizeof(itemCounts));
	for (i = 0; i < INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR; i++) Inventory.Table[i] = BLOCK_AIR;
	Inventory.SelectedIndex = 0;
	Inventory.Offset = 0;
	Survival_RefreshInventoryMap();
	Event_RaiseVoid(&UserEvents.HeldBlockChanged);
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

static cc_bool Survival_Tick(struct ScheduledTask2* task) {
	const float dt = task->interval;
	if (!Server.IsSinglePlayer || !World.Loaded) return true;

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

static void Survival_DrawMeter(int x, int y, int value, PackedCol fill) {
	int i, amount;
	PackedCol empty = PackedCol_Make(45, 45, 45, 230);
	PackedCol border = PackedCol_Make(0, 0, 0, 190);
	Gfx_Draw2DFlat(x - 2, y - 2, 102, 11, border);
	for (i = 0; i < 10; i++) {
		amount = value - i * 2;
		if (amount < 0) amount = 0;
		if (amount > 2) amount = 2;
		Gfx_Draw2DFlat(x + i * 10, y, 8, 7, empty);
		if (amount == 2) Gfx_Draw2DFlat(x + i * 10, y, 8, 7, fill);
		else if (amount == 1) Gfx_Draw2DFlat(x + i * 10, y, 4, 7, fill);
	}
}

static void Survival_RenderHUD(float delta) {
	int y, foodX;
	(void)delta;
	PackedCol healthCol = PackedCol_Make(210, 45, 45, 255);
	PackedCol foodCol = PackedCol_Make(222, 158, 45, 255);
	PackedCol airCol = PackedCol_Make(65, 150, 220, 255);
	if (!Survival_Active() || Game_HideGui) return;
	y = Game.Height - 54;
	if (y < 4) y = 4;
	foodX = Game.Width - 114;
	if (foodX < 12) foodX = 12;
	Survival_DrawMeter(12, y, health, healthCol);
	Survival_DrawMeter(foodX, y, food, foodCol);
	if (airSeconds < SURVIVAL_MAX_AIR) {
		int airValue = (int)((airSeconds / SURVIVAL_MAX_AIR) * 20.0f + 0.5f);
		Survival_DrawMeter((Game.Width - 100) / 2, y - 13, airValue, airCol);
	}
}

static void Survival_Init(void) {
	Survival_Reset();
	Game.Draw2DHooks[1] = Survival_RenderHUD;
	survivalTask.accumulator = 0.0f;
	survivalTask.interval = SURVIVAL_TICK;
	survivalTask.callback = Survival_Tick;
	survivalTask.next = NULL;
	ScheduledTask2_Add(&survivalTask);
}

static void Survival_Free(void) {
	if (Game.Draw2DHooks[1] == Survival_RenderHUD) Game.Draw2DHooks[1] = NULL;
}

struct IGameComponent Survival_Component = {
	Survival_Init,
	Survival_Free,
	Survival_Reset,
	Survival_OnNewMap,
	NULL,
	NULL
};
