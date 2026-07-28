#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Types.h"

namespace EditorUI
{
	// Render層へ渡す頂点
	struct UIVertex
	{
		DirectX::XMFLOAT2	Position;
		DirectX::XMFLOAT2	UV;
		Color32				Color;
	};

	// 白テクスチャ用
	using		TextureId				= uint64_t;
	constexpr	TextureId kWhiteTexture = 0;

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

		void Reset()
		{
			m_Vertices.clear();
			m_Indices.clear();
			m_Commands.clear();
			m_ClipRectStack.clear();
			m_TextureStack.clear();
			m_ClipRectStack.push_back(MakeInfiniteRect());
			m_TextureStack.push_back(kWhiteTexture);
		}

		void PushClipRect(const Rect2D& _clipRect) { m_ClipRectStack.push_back(_clipRect); }
		void PopClipRect()
		{
			assert(m_ClipRectStack.size() > 1 && "DrawList : 対応するPushClipRectがないPop");
			m_ClipRectStack.pop_back();
		}

		void PushTexture(TextureId _texture) { m_TextureStack.push_back(_texture); }
		void PopTexture()
		{
			assert(m_TextureStack.size() > 1 && "DrawList : 対応するPushTextureがないPop");
			m_TextureStack.pop_back();
		}

		void AddRectFilled(const Rect2D& _rect, Color32 _color)
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

		// 枠線は4本の矩形として積む
		void AddRectOutline(const Rect2D& _rect, Color32 _color, float _thickness)
		{
			AddRectFilled({ _rect.Min, {_rect.Max.x, _rect.Min.y + _thickness} },	_color);
			AddRectFilled({ {_rect.Min.x, _rect.Max.y - _thickness }, _rect.Max },	_color);
			AddRectFilled({ _rect.Min,{_rect.Min.x + _thickness, _rect.Max.y } },	_color);
			AddRectFilled({ {_rect.Max.x - _thickness,_rect.Min.y}, _rect.Max },	_color);
		}

		// リサイズグリップ等、矩形を表現できない形状用
		void AddTriangleFilled(const DirectX::XMFLOAT2& _point0, const DirectX::XMFLOAT2& _point1,
			const DirectX::XMFLOAT2& _point2, Color32 _color)
		{
			EnsureCommand();
			uint32_t base = static_cast<uint32_t>(m_Vertices.size());
			m_Vertices.push_back({ _point0,{0.0f,0.0f}, _color });
			m_Vertices.push_back({ _point1, {0.0f,0.0f}, _color });
			m_Vertices.push_back({ _point2, {0.0f,0.0f}, _color });
			m_Indices.insert(m_Indices.end(), { base, base + 1u, base + 2u });
			m_Commands.back().ElementCount += 3;
		}

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
