#include "BoxCollision.h"


bool IsHit(const AABB& _box1, const AABB& _box2)
{
	return !(
		_box1.Center.x + _box1.HalfExtents.x	< _box2.Center.x		||
		_box2.Center.x + _box2.HalfExtents.x	< _box1.Center.x		||
		_box1.Center.y + _box1.HalfExtents.y	< _box2.Center.y		||
		_box2.Center.y + _box2.HalfExtents.y	< _box1.Center.y		||
		_box1.Center.z + _box1.HalfExtents.z	< _box2.Center.z		||
		_box2.Center.z + _box2.HalfExtents.z	< _box1.Center.z);
}
