// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "DrawList.h"
#include <Engine/EditorUI/Text/Font/Font.h>

void EditorUI::DrawList::Reset()
{
	m_Vertices.clear();
	m_Indices.clear();
	m_Commands.clear();
	m_ClipRectStack.clear();
	m_TextureStack.clear();
	m_ClipRectStack.push_back(MakeInfiniteRect());
	m_TextureStack.push_back(kWhiteTexture);
}

void EditorUI::DrawList::PushClipRect(const Rect2D& _clipRect)
{
	m_ClipRectStack.push_back(_clipRect);
}

void EditorUI::DrawList::PopClipRect()
{
	assert(m_ClipRectStack.size() > 1 && "DrawList : 対応するPushClipRectがないPop");
	m_ClipRectStack.pop_back();
}

void EditorUI::DrawList::PushTexture(TextureId _texture)
{
	m_TextureStack.push_back(_texture);
}

void EditorUI::DrawList::PopTexture()
{
	assert(m_TextureStack.size() > 1 && "DrawList : 対応するPushTextureがないPop");
	m_TextureStack.pop_back();
}

void EditorUI::DrawList::AddRectFilled(const Rect2D& _rect, Color32 _color)
{
	EnsureCommand();
	uint32_t base = static_cast<uint32_t>(m_Vertices.size());
	m_Vertices.push_back({ {_rect.Min.x, _rect.Min.y}, {0.0f,0.0f}, _color });	// 左上
	m_Vertices.push_back({ {_rect.Max.x, _rect.Min.y}, {1.0f,0.0f}, _color });	// 右上
	m_Vertices.push_back({ {_rect.Max.x, _rect.Max.y}, {1.0f,1.0f}, _color });	// 右下
	m_Vertices.push_back({ {_rect.Min.x, _rect.Max.y}, {0.0f,1.0f}, _color });	// 左下

	m_Indices.insert(m_Indices.end(), { base, base + 1u, base + 2u, base, base + 2u, base + 3u });
	m_Commands.back().ElementCount += 6;
}

void EditorUI::DrawList::AddRectOutline(const Rect2D& _rect, Color32 _color, float _thickness)
{
	AddRectFilled({ _rect.Min, {_rect.Max.x, _rect.Min.y + _thickness} }, _color);
	AddRectFilled({ {_rect.Min.x, _rect.Max.y - _thickness }, _rect.Max }, _color);
	AddRectFilled({ _rect.Min,{_rect.Min.x + _thickness, _rect.Max.y } }, _color);
	AddRectFilled({ {_rect.Max.x - _thickness,_rect.Min.y}, _rect.Max }, _color);
}

void EditorUI::DrawList::AddTriangleFilled(
	const DirectX::XMFLOAT2&	_point0, 
	const DirectX::XMFLOAT2&	_point1, 
	const DirectX::XMFLOAT2&	_point2, 
	Color32						_color)
{
	EnsureCommand();
	uint32_t base = static_cast<uint32_t>(m_Vertices.size());
	m_Vertices.push_back({ _point0,{0.0f,0.0f}, _color });
	m_Vertices.push_back({ _point1, {0.0f,0.0f}, _color });
	m_Vertices.push_back({ _point2, {0.0f,0.0f}, _color });
	m_Indices.insert(m_Indices.end(), { base, base + 1u, base + 2u });
	m_Commands.back().ElementCount += 3;
}

void EditorUI::DrawList::AddGlyphQuad(const Rect2D& _bounds, const Rect2D& _uv, Color32 _color)
{
	EnsureCommand();
	uint32_t base = static_cast<uint32_t>(m_Vertices.size());
	m_Vertices.push_back({ { _bounds.Min.x, _bounds.Min.y }, { _uv.Min.x, _uv.Min.y }, _color });
	m_Vertices.push_back({ { _bounds.Max.x, _bounds.Min.y }, { _uv.Max.x, _uv.Min.y }, _color });
	m_Vertices.push_back({ { _bounds.Max.x, _bounds.Max.y }, { _uv.Max.x, _uv.Max.y }, _color });
	m_Vertices.push_back({ { _bounds.Min.x, _bounds.Max.y }, { _uv.Min.x, _uv.Max.y }, _color });
	m_Indices.insert(m_Indices.end(), { base, base + 1u, base + 2u, base, base + 2u, base + 3u });
	m_Commands.back().ElementCount += 6;
}

void EditorUI::DrawList::AddText(const DirectX::XMFLOAT2& _pos, Color32 _color, std::wstring_view _text, Font& _font)
{
	PushTexture(_font.GetTextureId());

	float penX = _pos.x;
	for (wchar_t ch : _text)
	{
		const Glyph* pGlyph = _font.GetGlyph(ch);
		if (pGlyph == nullptr)
		{
			continue; // 未対応文字(アトラス満杯等)は空白扱いでスキップする
		}

		Rect2D bounds = MakeRect({ penX + pGlyph->Bearing.x, _pos.y + pGlyph->Bearing.y }, pGlyph->Size);
		AddGlyphQuad(bounds, pGlyph->UV, _color);

		penX += pGlyph->Advance;
	}

	PopTexture();
}
