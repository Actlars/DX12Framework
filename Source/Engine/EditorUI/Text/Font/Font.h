#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Types.h>
#include <Engine/RHI/Resource/Texture/Texture.h>
#include <dwrite.h>

namespace RHI { class Device; }
class EditorUIRenderer;

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// Glyph struct
	// 
	// 概要 : 
	//	1文字分のアトラス内でのレイアウト情報
	// -------------------------------------------------------------------------------
	struct Glyph
	{
		Rect2D				UV{};			// アトラス内のUV座標(0.0 ～ 1.0に正規化済み)
		DirectX::XMFLOAT2	Size{};			// 描画時の矩形サイズ
		DirectX::XMFLOAT2	Bearing{};		// ペン位置から見たグリフ左上のオフセット
		float				Advance = 0.0f;	// 次の文字へ進む幅
	};

	// -------------------------------------------------------------------------------
	// Font class
	// 
	// 概要 : 
	//	DirectWriteでシステムフォントからグリフをラスタライズし、1枚のテクスチャ(アトラス)にまとめて保持するクラス
	//	GetGlyphで初めて使われた文字をその場でラスタライズしてアトラスに追記する方式をとる
	// -------------------------------------------------------------------------------
	class Font
	{
	public:

		void Term();

		// -------------------------------------------------------------------------------
		// @brief	フォントを読み込み、アトラス用のGPUテクスチャを確保する
		// 
		// @param[in]	_familyName		システムフォント名
		// @param[in]	_fontSizePx		フォントサイズ
		// @param[in]	_pDevice		DX12デバイス
		// @param[in]	_pRenderer		テクスチャ登録先
		// @param[in]	_atlasWidth		アトラステクスチャの幅
		// @param[in]	_atlasHeight	アトラステクスチャの高さ
		// @retval	true	成功
		// @retval	false	失敗
		// -------------------------------------------------------------------------------
		bool Build(
			const std::wstring& _familyName,
			float				_fontSizePx,
			RHI::Device*		_pDevice,
			EditorUIRenderer*	_pRenderer,
			uint32_t			_atlasWidth = 1024,
			uint32_t			_atlasHeight = 1024);

		// -------------------------------------------------------------------------------
		// @brief	文字1つ分のグリフ情報を取得する。未登録の文字は、この呼び出しの中で
		//			その場でラスタライズしてアトラスに追記する
		// 
		// @param[in]	_ch		取得したい文字
		// @return	取得できた場合はGlyphへのポインタ、アトラスが満杯で追記できない等
		//			失敗時はnullptr
		// -------------------------------------------------------------------------------
		const Glyph* GetGlyph(wchar_t _ch);

		// -------------------------------------------------------------------------------
		// @brief	GetGlyphでの追記により汚れたアトラスを、GPUテクスチャへ再アップロードする
		//			1フレームの中で複数回GetGlyphが呼ばれても、アップロードはこの関数の呼び出し1回にまとめて行われる
		// 
		// @param[in]	_pDevice	DX12デバイス
		// -------------------------------------------------------------------------------
		void Flush(RHI::Device* _pDevice);

		// -------------------------------------------------------------------------------
		// @brief	行の高さを返す
		// -------------------------------------------------------------------------------
		float GetLineHeight() const { return m_LineHeight; }

		// -------------------------------------------------------------------------------
		// @brief	このフォントのアトラスが登録されているテクスチャIDを返す
		// -------------------------------------------------------------------------------
		TextureId GetTextureId() const { return m_TextureId; }

	private:

		// 1文字をラスタライズし、アトラス内の空き位置に配置する
		bool RasterizeGlyph(wchar_t _ch, Glyph& _outGlyph);

		// シェルフ方式の簡易パッカー。指定サイズが入る空き位置を探し、
		// 見つかればアトラス内座標(ピクセル)を返す。満杯ならfalse
		bool PlaceInAtlas(int _width, int _height, int& _outX, int& _outY);

		// アトラスの初回GPUテクスチャ生成(DEFAULTヒープ)
		bool CreateAtlasTexture(RHI::Device* _pDevice, EditorUIRenderer* _pRenderer);

		// CPU側アトラス画像の全体を、専用の使い捨てコマンドリストでGPUへ書き込む
		void UploadAtlasTexture(RHI::Device* _pDevice);

		ComPtr<IDWriteFactory>			m_pDWriteFactory;
		ComPtr<IDWriteFontFace>			m_pFontFace;
		ComPtr<IDWriteGdiInterop>		m_pGdiInterop;
		ComPtr<IDWriteRenderingParams>	m_pRenderingParams;

		float m_FontSizePx		= 16.0f;
		float m_LineHeight		= 0.0f;
		float m_AscentPx		= 0.0f;
		float m_UnitsPerEmScale = 0.0f;

		std::unordered_map<wchar_t, Glyph> m_Glyphs;

		// CPU側に常駐させるアトラス画像
		// 新しい文字が来るたびにここへ直接書き込み、FlushでGPUへ反映する
		std::vector<uint8_t>	m_AtlasPixels;
		uint32_t				m_AtlasWidth	= 0;
		uint32_t				m_AtlasHeight	= 0;
		bool					m_Dirty			= false;

		// シェルフパッカーの状態(次に配置する位置)
		struct ShelfCursor
		{
			int X = 0;
			int Y = 0;
			int ShelfHeight = 0;
		};
		ShelfCursor m_Cursor;

		RHI::Texture	m_AtlasTextureObj;	// アトラスのGPUリソース + SRVを保持する
		TextureId		m_TextureId = 0;
	};
}
