#include "DebugLineDrawItem.h"

namespace
{
	const D3D12_INPUT_ELEMENT_DESC kInputElements[]
	{
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT,
			0,
			0,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};
}

const D3D12_INPUT_LAYOUT_DESC DebugLineVertex::InputLayout
{
	kInputElements,
	_countof(kInputElements)
};
