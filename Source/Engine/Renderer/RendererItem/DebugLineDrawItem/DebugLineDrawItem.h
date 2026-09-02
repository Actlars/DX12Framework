#pragma once

// -------------------------------------------------------------------------------
// DebugLineVertex
//
// 概要 :
//  デバッグライン用頂点構造体
// -------------------------------------------------------------------------------
struct DebugLineVertex
{
	Math::Vector3 Position;
	Math::Vector4 Color = { 1.0f,0.0f,0.0f,1.0f };

	static const D3D12_INPUT_LAYOUT_DESC InputLayout;

private:
	static constexpr uint32_t InputElementCount = 2;
	static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};

static_assert(sizeof(DebugLineVertex) == 28, "DebugLineVertex size mismatch");


struct DebugLineDrawItem
{
	Math::Vector3 Start = { 0.0f,0.0f,0.0f };
	Math::Vector3 End = { 1.0f,1.0f,1.0f };

	Math::Vector4 Color
	{
		1.0f,
		0.0f,
		0.0f,
		1.0f,
	};
};