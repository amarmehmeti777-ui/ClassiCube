#ifndef CC_SURVIVAL_H
#define CC_SURVIVAL_H
#include "Core.h"
#include "BlockID.h"
#include "String_.h"
CC_BEGIN_HEADER

struct LocalPlayer;
struct IGameComponent;
extern struct IGameComponent Survival_Component;

int  Survival_GetHealth(void);
int  Survival_GetFood(void);
int  Survival_GetAir(void);
cc_bool Survival_IsActive(void);
int  Survival_GetItemCount(BlockID block);
int  Survival_InventoryAdd(BlockID block, int amount);
cc_bool Survival_CanPlaceBlock(BlockID block);
cc_bool Survival_UseItem(BlockID block);
cc_bool Survival_AttackMob(void);
void Survival_OnBlockBroken(BlockID block, int x, int y, int z);
void Survival_OnBlockPlaced(BlockID block);
cc_bool Survival_MineBlock(int x, int y, int z, BlockID block, float elapsed);
void Survival_CancelMining(void);
float Survival_GetMiningProgress(void);
void Survival_RenderItemCount(int x, int y, BlockID block);
cc_bool Survival_IsDead(void);
void Survival_Damage(int amount);
void Survival_Heal(int amount);
void Survival_AddFood(int foodPoints, float saturation);
void Survival_AddExhaustion(float amount);
void Survival_OnPlayerPhysics(struct LocalPlayer* player);
void Survival_Reset(void);
cc_result Survival_Save(const cc_string* mapPath);
cc_result Survival_Load(const cc_string* mapPath);

CC_END_HEADER
#endif
