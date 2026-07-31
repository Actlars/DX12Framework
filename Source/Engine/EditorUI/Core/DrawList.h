#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Types.h"

namespace EditorUI
{
	class Font;

	// Render層へ渡す頂点
	struct UIVertex
	{
		DirectX::XMFLOAT2	Position;
		DirectX::XMFLOAT2	UV;
		Color32				Color;
	};

	// 連続する同一テクスチャ/同一クリップ矩形の描画コマンドをまとめた1バッチ
	struct DrawCommand
	{
		uint32_t	IndexOffset		= 0;
		uint32_t	ElementCount	= 0;
		Rect2D		ClipRect;
		TextureId	Texture			= kWhiteTexture;
	};

	// 1フレーム使い捨ての頂点・インデックス・描画コマンドバッファ
	class DrawList
	{
	public:

		DrawList() {}
		~DrawList() {}

		void Reset();

		void PushClipRect(const Rect2D& _clipRect);
		void PopClipRect();

		void PushTexture(TextureId _texture);
		void PopTexture();

		void AddRectFilled(const Rect2D& _rect, Color32 _color);

		// 枠線は4本の矩形として積む
		void AddRectOutline(const Rect2D& _rect, Color32 _color, float _thickness);

		// リサイズグリップ等、矩形を表現できない形状用
		void AddTriangleFilled(const DirectX::XMFLOAT2& _point0, const DirectX::XMFLOAT2& _point1,
			const DirectX::XMFLOAT2& _point2, Color32 _color);

		// UV座標を明示指定できる矩形。フォントアトラスの1グリフ分を描くのに使う。
		// AddRectFilledはUVが常に(0,0)-(1,1)固定なので、それとは別に用意する ---
		void AddGlyphQuad(const Rect2D& _bounds, const Rect2D& _uv, Color32 _color);

		// 文字列を1文字ずつ、フォントのアトラスからグリフを引いて並べて描く ---
		void AddText(const DirectX::XMFLOAT2& _pos, Color32 _color, std::wstring_view _text, Font& _font);

		const std::vector<UIVertex>&	GetVertices()	const { return m_Vertices;	}
		const std::vector<uint32_t>&	GetIndices()	const { return m_Indices;	}
		const std::vector<DrawCommand>& GetCommands()	const { return m_Commands;	}
		
	private:

		// 直前のコマンドと同じテクスチャ・クリップ矩形ならバッチに追加、違えば新規コマンドを作る
		void EnsureCommand()
		{
			const Rect2D& clip = m_ClipRectStack.back();
			const TextureId tex = m_TextureStack.back();

			if (m_Commands.empty() ||
				m_Commands.back().Texture != tex ||
				!Rect2D::Equals(m_Commands.back().ClipRect, clip))
			{
				m_Commands.push_back({ static_cast<uint32_t>(m_Indices.size()), 0 , clip, tex });
			}
		}

		std::vector<UIVertex>		m_Vertices;
		std::vector<uint32_t>		m_Indices;
		std::vector<DrawCommand>	m_Commands;
		std::vector<Rect2D>			m_ClipRectStack;
		std::vector<TextureId>		m_TextureStack;

	};
}
