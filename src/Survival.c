#include "Survival.h"
#include "BlockID.h"
#include "Chat.h"
#include "Entity.h"
#include "ExtMath.h"
#include "Game.h"
#include "Inventory.h"
#include "Event.h"
#include "Errors.h"
#include "String_.h"
#include "Platform.h"
#include "Stream.h"
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
static cc_bool miningActive;
static int miningX, miningY, miningZ;
static float timeOfDay;
static int dayCycleStep;
static BlockID miningBlock;
static float miningProgress;

cc_bool Survival_IsActive(void) {
	return Server.IsSinglePlayer && !Game_ClassicMode && World.Loaded;
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

static float Survival_GetBlockMiningTime(BlockID block) {
	switch (block) {
	case BLOCK_OBSIDIAN: return 8.0f;
	case BLOCK_STONE:
	case BLOCK_COBBLE:
	case BLOCK_GOLD_ORE:
	case BLOCK_IRON_ORE:
	case BLOCK_COAL_ORE:
	case BLOCK_BRICK:
	case BLOCK_STONE_BRICK:
	case BLOCK_MOSSY_ROCKS: return 2.0f;
	case BLOCK_LOG:
	case BLOCK_WOOD:
	case BLOCK_BOOKSHELF:
	case BLOCK_CRATE: return 1.0f;
	case BLOCK_DIRT:
	case BLOCK_GRASS:
	case BLOCK_SAND:
	case BLOCK_GRAVEL:
	case BLOCK_SNOW: return 0.5f;
	case BLOCK_LEAVES:
	case BLOCK_SAPLING:
	case BLOCK_DANDELION:
	case BLOCK_ROSE:
	case BLOCK_BROWN_SHROOM:
	case BLOCK_RED_SHROOM: return 0.5f;
	default: return 1.0f;
	}
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
	timeOfDay = 0.0f;
	dayCycleStep = -1;
	Mem_Set(itemCounts, 0, sizeof(itemCounts));
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
	cc_uint8 data[12 + BLOCK_COUNT * 4 + INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR * 2];
	int i, itemTypes = 0, offset = 12;
	cc_result res;

	if (!Survival_IsActive()) return 0;
	if ((res = Survival_MakeSidecarPath(&path, pathBuffer, mapPath))) return res;

	for (i = 1; i < BLOCK_COUNT; i++) {
		if (itemCounts[i] > 0) itemTypes++;
	}
	data[0] = 'C'; data[1] = 'C'; data[2] = 'S'; data[3] = 'V';
	data[4] = 1;
	data[5] = (cc_uint8)max(0, min(health, SURVIVAL_MAX_HEALTH));
	data[6] = (cc_uint8)max(0, min(food, SURVIVAL_MAX_FOOD));
	data[7] = (cc_uint8)max(0, min((int)(airSeconds + 0.5f), 15));
	data[8] = (cc_uint8)max(0, min((int)(saturation + 0.5f), SURVIVAL_MAX_FOOD));
	data[9] = (cc_uint8)min(itemTypes, SURVIVAL_INVENTORY_SLOTS);
	data[10] = (cc_uint8)max(0, min(Inventory.SelectedIndex, HOTBAR_MAX_INDEX));
	data[11] = (cc_uint8)max(0, min(Inventory.Offset / INVENTORY_BLOCKS_PER_HOTBAR, INVENTORY_HOTBARS - 1));

	itemTypes = 0;
	for (i = 1; i < BLOCK_COUNT && itemTypes < data[9]; i++) {
		cc_uint16 block, count;
		if (itemCounts[i] <= 0) continue;
		block = (cc_uint16)i;
		count = (cc_uint16)min(itemCounts[i], SURVIVAL_INVENTORY_SLOTS * SURVIVAL_INVENTORY_STACK_SIZE);
		data[offset++] = (cc_uint8)(block & 0xFF);
		data[offset++] = (cc_uint8)(block >> 8);
		data[offset++] = (cc_uint8)(count & 0xFF);
		data[offset++] = (cc_uint8)(count >> 8);
		itemTypes++;
	}

	for (i = 0; i < INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR; i++) {
		cc_uint16 block = (cc_uint16)Inventory.Table[i];
		data[offset++] = (cc_uint8)(block & 0xFF);
		data[offset++] = (cc_uint8)(block >> 8);
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
	cc_uint8 data[12 + BLOCK_COUNT * 4 + INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR * 2];
	cc_uint32 payloadSize;
	int i, offset, stackSlots = 0;
	cc_result res;

	if (!Survival_IsActive()) return 0;
	if ((res = Survival_MakeSidecarPath(&path, pathBuffer, mapPath))) return res;
	Platform_EncodePath(&rawPath, &path);
	res = Stream_OpenPath(&stream, &rawPath);
	/* Older maps have no companion file; keep their fresh Survival state. */
	if (res) return 0;

	res = Stream_Read(&stream, data, 12);
	if (res) {
		stream.Close(&stream);
		return res;
	}
	if (data[0] != 'C' || data[1] != 'C' || data[2] != 'S' || data[3] != 'V' ||
		data[4] != 1 || data[9] > SURVIVAL_INVENTORY_SLOTS ||
		data[10] > HOTBAR_MAX_INDEX || data[11] >= INVENTORY_HOTBARS) {
		stream.Close(&stream);
		return ERR_INVALID_ARGUMENT;
	}

	payloadSize = (cc_uint32)data[9] * 4U +
		(cc_uint32)INVENTORY_HOTBARS * INVENTORY_BLOCKS_PER_HOTBAR * 2U;
	res = Stream_Read(&stream, data + 12, payloadSize);
	if (res) {
		stream.Close(&stream);
		return res;
	}
	res = stream.Close(&stream);
	if (res) return res;

	Mem_Set(itemCounts, 0, sizeof(itemCounts));
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
	for (i = 0; i < data[9]; i++) {
		cc_uint16 block = (cc_uint16)(data[offset] | ((cc_uint16)data[offset + 1] << 8));
		cc_uint16 amount = (cc_uint16)(data[offset + 2] | ((cc_uint16)data[offset + 3] << 8));
		offset += 4;
		if (block == BLOCK_AIR || block >= BLOCK_COUNT || amount == 0) continue;
		itemCounts[block] = min((int)amount, SURVIVAL_INVENTORY_SLOTS * SURVIVAL_INVENTORY_STACK_SIZE);
		stackSlots += (itemCounts[block] + SURVIVAL_INVENTORY_STACK_SIZE - 1) / SURVIVAL_INVENTORY_STACK_SIZE;
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
	if (!Server.IsSinglePlayer || !World.Loaded) return true;
	Survival_UpdateDayNight(dt);

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
	if (miningActive) {
		int barX = (Game.Width - 66) / 2;
		int barY = Game.Height / 2 + 18;
		int barWidth = (int)(64.0f * Survival_GetMiningProgress());
		Gfx_Draw2DFlat(barX - 1, barY - 1, 66, 7, PackedCol_Make(0, 0, 0, 200));
		if (barWidth > 0) Gfx_Draw2DFlat(barX, barY, barWidth, 5, PackedCol_Make(235, 180, 65, 255));
	}
}

static void Survival_Init(void) {
	Survival_Reset();
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
