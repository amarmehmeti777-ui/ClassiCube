#ifndef CC_SURVIVAL_H
#define CC_SURVIVAL_H
#include "Core.h"
#include "BlockID.h"
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
void Survival_OnBlockBroken(BlockID block);
void Survival_OnBlockPlaced(BlockID block);
void Survival_RenderItemCount(int x, int y, BlockID block);
cc_bool Survival_IsDead(void);
void Survival_Damage(int amount);
void Survival_Heal(int amount);
void Survival_AddFood(int foodPoints, float saturation);
void Survival_AddExhaustion(float amount);
void Survival_OnPlayerPhysics(struct LocalPlayer* player);
void Survival_Reset(void);

CC_END_HEADER
#endif
