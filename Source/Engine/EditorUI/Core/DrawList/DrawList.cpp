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

void EditorUI::DrawList::PushClipRect(const Rect2D& _clipRect, bool _intersectWithParent)
{
	// 子Clipが親Clipの外へ広がらないよう、現在Clipとの積を積む
	// ここで積を取らないと、ウィジェットが自分の矩形を積んだ瞬間に
	// 親ウィンドウのクリップが無効化され、文字がウィンドウ外へはみ出す
	const Rect2D clip = _intersectWithParent
		? Rect2D::Intersect(m_ClipRectStack.back(), _clipRect)
		: _clipRect;

	m_ClipRectStack.push_back(clip);
}

const EditorUI::Rect2D& EditorUI::DrawList::GetClipRect() const
{
	return m_ClipRectStack.back();
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

// -------------------------------------------------------------------------------
// 塗りつぶし矩形を描画リストに追加
// -------------------------------------------------------------------------------
void EditorUI::DrawList::AddRectFilled(const Rect2D& _rect, Color32 _color)
{
	// 現在のTexture/ClipRectに対応する描画コマンドを用意
	EnsureCommand();
	// 今から描画する矩形の先頭の頂点Indexを取得
	// 既存の頂点数を基準にIndexBufferを作成する
	uint32_t base = static_cast<uint32_t>(m_Vertices.size());
	// 矩形を構成する4頂点を時計回りに追加
	// 各頂点には座標・UV座標・頂点カラーを設定
	m_Vertices.push_back({ {_rect.Min.x, _rect.Min.y}, {0.0f,0.0f}, _color });	// 左上
	m_Vertices.push_back({ {_rect.Max.x, _rect.Min.y}, {1.0f,0.0f}, _color });	// 右上
	m_Vertices.push_back({ {_rect.Max.x, _rect.Max.y}, {1.0f,1.0f}, _color });	// 右下
	m_Vertices.push_back({ {_rect.Min.x, _rect.Max.y}, {0.0f,1.0f}, _color });	// 左下

	// 4頂点から2枚の三角形を作成し、1つの矩形として描画できるようにIndexを追加する
	// 三角形1 左上→右上→右下
	// 三角形2 左上→右下→左下
	m_Indices.insert(m_Indices.end(), { base, base + 1u, base + 2u, base, base + 2u, base + 3u });
	// 矩形1枚は2三角形 = 6Indexを使用するため、現在の描画コマンドの描画要素数を6増やす
	m_Commands.back().ElementCount += 6;
}

// -------------------------------------------------------------------------------
// 矩形の枠線を描画リストに追加
// -------------------------------------------------------------------------------
void EditorUI::DrawList::AddRectOutline(const Rect2D& _rect, Color32 _color, float _thickness)
{
	AddRectFilled({ _rect.Min, {_rect.Max.x, _rect.Min.y + _thickness} }, _color);
	AddRectFilled({ {_rect.Min.x, _rect.Max.y - _thickness }, _rect.Max }, _color);
	AddRectFilled({ _rect.Min,{_rect.Min.x + _thickness, _rect.Max.y } }, _color);
	AddRectFilled({ {_rect.Max.x - _thickness,_rect.Min.y}, _rect.Max }, _color);


	// 下記のコードで頂点数を半分にすることは可能だが、必要になったら変更する
	//EnsureCommand();

	//const uint32_t base =
	//	static_cast<uint32_t>(m_Vertices.size());

	//// 外側4頂点
	//m_Vertices.push_back({ {_rect.Min.x, _rect.Min.y}, {0.0f, 0.0f}, _color }); // 0
	//m_Vertices.push_back({ {_rect.Max.x, _rect.Min.y}, {1.0f, 0.0f}, _color }); // 1
	//m_Vertices.push_back({ {_rect.Max.x, _rect.Max.y}, {1.0f, 1.0f}, _color }); // 2
	//m_Vertices.push_back({ {_rect.Min.x, _rect.Max.y}, {0.0f, 1.0f}, _color }); // 3

	//// 内側4頂点
	//m_Vertices.push_back({{_rect.Min.x + _thickness, _rect.Min.y + _thickness},{0.0f, 0.0f},_color}); // 4
	//m_Vertices.push_back({{_rect.Max.x - _thickness, _rect.Min.y + _thickness},{1.0f, 0.0f},_color}); // 5
	//m_Vertices.push_back({{_rect.Max.x - _thickness, _rect.Max.y - _thickness},{1.0f, 1.0f},_color}); // 6
	//m_Vertices.push_back({{_rect.Min.x + _thickness, _rect.Max.y - _thickness},{0.0f, 1.0f},_color}); // 7

	//m_Indices.insert(
	//	m_Indices.end(),
	//	{
	//		// 上
	//		base + 0, base + 1, base + 5,base + 0, base + 5, base + 4,
	//		// 右
	//		base + 1, base + 2, base + 6,base + 1, base + 6, base + 5,
	//		// 下
	//		base + 2, base + 3, base + 7,base + 2, base + 7, base + 6,
	//		// 左
	//		base + 3, base + 0, base + 4,base + 3, base + 4, base + 7
	//	});

	//m_Commands.back().ElementCount += 24;
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

// -------------------------------------------------------------------------------
// 任意のテクスチャを矩形へ貼る
// -------------------------------------------------------------------------------
void EditorUI::DrawList::AddImage(const Rect2D& _rect, TextureId _texture, const Rect2D& _uv, Color32 _tint)
{
	// 白テクスチャ以外を使うため、このQuadの間だけテクスチャを差し替える
	PushTexture(_texture);
	EnsureCommand();

	uint32_t base = static_cast<uint32_t>(m_Vertices.size());
	m_Vertices.push_back({ { _rect.Min.x, _rect.Min.y }, { _uv.Min.x, _uv.Min.y }, _tint });	// 左上
	m_Vertices.push_back({ { _rect.Max.x, _rect.Min.y }, { _uv.Max.x, _uv.Min.y }, _tint });	// 右上
	m_Vertices.push_back({ { _rect.Max.x, _rect.Max.y }, { _uv.Max.x, _uv.Max.y }, _tint });	// 右下
	m_Vertices.push_back({ { _rect.Min.x, _rect.Max.y }, { _uv.Min.x, _uv.Max.y }, _tint });	// 左下

	m_Indices.insert(m_Indices.end(), { base, base + 1u, base + 2u, base, base + 2u, base + 3u });
	m_Commands.back().ElementCount += 6;

	PopTexture();
}

// -------------------------------------------------------------------------------
// 1文字分のGlyphQuadを描画リストへ追加
// -------------------------------------------------------------------------------
void EditorUI::DrawList::AddGlyphQuad(const Rect2D& _bounds, const Rect2D& _uv, Color32 _color)
{
	// 現在のTexture/ClipRectに対応する描画コマンドを用意する
	EnsureCommand();
	// 追加する4頂点の先頭Indexを取得
	uint32_t base = static_cast<uint32_t>(m_Vertices.size());
	// 文字を描画する画面上の矩形とFontAtlas上のUV領域を対応付けて4頂点を追加する
	m_Vertices.push_back({ { _bounds.Min.x, _bounds.Min.y }, { _uv.Min.x, _uv.Min.y }, _color });	// 左上
	m_Vertices.push_back({ { _bounds.Max.x, _bounds.Min.y }, { _uv.Max.x, _uv.Min.y }, _color });	// 右上
	m_Vertices.push_back({ { _bounds.Max.x, _bounds.Max.y }, { _uv.Max.x, _uv.Max.y }, _color });	// 右下
	m_Vertices.push_back({ { _bounds.Min.x, _bounds.Max.y }, { _uv.Min.x, _uv.Max.y }, _color });	// 左下
	// 4頂点から2枚の三角形を作成して、1つの文字用Quadとして描画する
	m_Indices.insert(m_Indices.end(), { base, base + 1u, base + 2u, base, base + 2u, base + 3u });
	// Quad1枚は6個のIndexを使用
	m_Commands.back().ElementCount += 6;
}

// -------------------------------------------------------------------------------
// 文字列を描画リストに追加
// -------------------------------------------------------------------------------
void EditorUI::DrawList::AddText(const DirectX::XMFLOAT2& _pos, Color32 _color, std::wstring_view _text, Font& _font)
{
	// 文字描画ではFontAtlasTextureを使用するため
	// 現在の描画TextureとしてFontのTextureIdを設定する
	PushTexture(_font.GetTextureId());

	// 現在の文字を描画するX座標
	// 最初は指定された描画開始位置から始める
	float penX = _pos.x;
	// 文字列を1文字ずつ処理する
	for (wchar_t ch : _text)
	{
		// 文字に対応するGlyph情報をFontから取得
		const Glyph* pGlyph = _font.GetGlyph(ch);
		// 対応するGlyphが存在しない場合は描画せずスキップする
		if (pGlyph == nullptr)
		{
			continue; // 未対応文字(アトラス満杯等)は空白扱いでスキップする
		}
		// GlyphのBearingを考慮して、実際に文字画像を配置する画面上の矩形を作成する
		Rect2D bounds = MakeRect({ penX + pGlyph->Bearing.x, _pos.y + pGlyph->Bearing.y }, pGlyph->Size);
		// FontAtlas上のGlyphのUV領域を使って1文字分のQuadを描画リストへ追加する
		AddGlyphQuad(bounds, pGlyph->UV, _color);

		// 次の文字を描画する位置へ進める
		// Glyphごとに文字幅が異なるためAdvance値を使用する
		penX += pGlyph->Advance;
	}
	// FontAtlasTextureの使用を終了し、以前使用していたTextureへ戻す
	PopTexture();
}

// -------------------------------------------------------------------------------
// 現在の描画状態に対するDrawCommandを用意
// -------------------------------------------------------------------------------
void EditorUI::DrawList::EnsureCommand()
{
	// 現在有効になっているクリップ領域を取得
	const Rect2D&	clip = m_ClipRectStack.back();
	// 現在使用するテクスチャを取得
	const TextureId tex = m_TextureStack.back();

	// 以下のいずれかに当てはまる場合は新しい描画コマンドを作成する
	// 1. まだ描画コマンドが1つもない
	// 2. 前回の描画コマンドと使用テクスチャが異なる
	// 3. 前回の描画コマンドとクリップ領域が異なる
	if (m_Commands.empty() ||
		m_Commands.back().Texture != tex ||
		!Rect2D::Equals(m_Commands.back().ClipRect, clip))
	{
		// 現在のIndexBuffer位置を開始地点として新しい描画コマンドを追加する
		// ElementCountはまだ描画要素が追加されていないため0
		m_Commands.push_back({ static_cast<uint32_t>(m_Indices.size()), 0 , clip, tex });
	}
}
