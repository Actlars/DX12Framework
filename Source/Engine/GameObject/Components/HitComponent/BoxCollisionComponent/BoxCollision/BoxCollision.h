#pragma once
#include <Engine/GameObject/Components/HitComponent/BoxCollisionComponent/BoxCollision/HitBox.h>

// -------------------------------------------------------------------------------
// AABBのボックス当たり判定
// @param[in]	_box1	判定するボックス1
// @param[in]	_box2	判定するボックス2
// @return		true	当たっている
// @return		false	当たっていない
// -------------------------------------------------------------------------------
bool IsHit(const AABB& _box1, const AABB& _box2);