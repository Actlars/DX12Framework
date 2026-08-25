#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Types.h>

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
		~DrawList() = default;

		void Reset();

		// -------------------------------------------------------------------------------
		// @brief	クリップ矩形を積む
		//
		//	既定では現在のクリップとの積集合を積む
		//	そうしないと、内側の部品が自分の矩形をそのまま積んだ時点で
		//	親ウィンドウの範囲を超えて描けてしまう(はみ出した文字が消えない原因)
		//
		// @param[in]	_clipRect				積みたい矩形
		// @param[in]	_intersectWithParent	false を渡した場合のみ親を無視する
		//										ウィンドウの外に出るオーバーレイ専用
		// -------------------------------------------------------------------------------
		void PushClipRect(const Rect2D& _clipRect, bool _intersectWithParent = true);
		void PopClipRect();

		// 現在有効なクリップ矩形。ウィジェットが自分で可視判定を行うために使う
		const Rect2D& GetClipRect() const;

		void PushTexture(TextureId _texture);
		void PopTexture();

		void AddRectFilled(const Rect2D& _rect, Color32 _color);
		// 枠線は4本の矩形として積む
		void AddRectOutline(const Rect2D& _rect, Color32 _color, float _thickness);
		// リサイズグリップ等、矩形を表現できない形状用
		void AddTriangleFilled(
			const DirectX::XMFLOAT2&	_point0, 
			const DirectX::XMFLOAT2&	_point1,
			const DirectX::XMFLOAT2&	_point2, 
			Color32						_color);

		// -------------------------------------------------------------------------------
		// @brief	任意のテクスチャを矩形に貼る
		//
		//	ゲーム画面(オフスクリーンのレンダーターゲット)や
		//	コンテンツブラウザのサムネイルを描くために使う
		//
		// @param[in]	_rect		貼り付け先の矩形
		// @param[in]	_texture	Rendererに登録済みのテクスチャId
		// @param[in]	_uv			サンプリングするUV範囲
		// @param[in]	_tint		乗算する色。白(不透明)ならテクスチャそのまま
		// -------------------------------------------------------------------------------
		void AddImage(
			const Rect2D&	_rect,
			TextureId		_texture,
			const Rect2D&	_uv		= Rect2D{ {0.0f,0.0f},{1.0f,1.0f} },
			Color32			_tint	= 0xFFFFFFFFu);

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
		void EnsureCommand();

		std::vector<UIVertex>		m_Vertices;
		std::vector<uint32_t>		m_Indices;
		std::vector<DrawCommand>	m_Commands;
		std::vector<Rect2D>			m_ClipRectStack;
		std::vector<TextureId>		m_TextureStack;

	};
}
