#pragma once

#include <include/SimpleMath.h>

// DirectX::SimpleMathの型をMath名前空間にする
namespace Math
{
	using Vector2		= DirectX::SimpleMath::Vector2;
	using Vector3		= DirectX::SimpleMath::Vector3;
	using Vector4		= DirectX::SimpleMath::Vector4;
	using Matrix		= DirectX::SimpleMath::Matrix;
	using Quaternion	= DirectX::SimpleMath::Quaternion;
}
