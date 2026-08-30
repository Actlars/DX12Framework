// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "Font.h"
#include <Engine/EditorUI/Render/EditorUIRenderer/EditorUIRenderer.h>
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/Utility/Debug/Logger/Logger.h>
#pragma comment(lib, "Dwrite.lib")

namespace
{
	constexpr int kCellPadding = 1;	// 隣接グリフのピクセルが混ざらないようにする余白

	// -------------------------------------------------------------------------------
	// 送り幅(advance)からはみ出して描かれる分の余白
	//
	// 'W' や斜体のように、文字の絵が送り幅より広いグリフがある
	// セルを送り幅ちょうどにすると端が切れるため、左右に少しだけ余裕を持たせる
	// -------------------------------------------------------------------------------
	constexpr int kGlyphOverhang = 2;
}

void EditorUI::Font::Term()
{
	m_AtlasTextureObj.Term();
	m_Glyphs.clear();
	m_pFontFace.Reset();
	m_pGdiInterop.Reset();
	m_pRenderingParams.Reset();
	m_pDWriteFactory.Reset();
	m_TextureId = 0;
}

// -------------------------------------------------------------------------------
// フォントの初期化
// DirectXWriteから指定フォントを取得し、フォントメトリクスを計算したうえで、
// CPU側のフォントアトラスとGPU側のアトラステクスチャを準備する
// -------------------------------------------------------------------------------
bool EditorUI::Font::Build(
	const std::wstring& _familyName,
	float				_fontSizePx,
	RHI::Device*		_pDevice, 
	EditorUIRenderer*	_pRenderer,
	uint32_t			_atlasWidth, 
	uint32_t			_atlasHeight)
{
	// GPUリソース作成にはDeviceが必要
	// アトラステクスチャをEditorUIから使用するにはRendererへの登録も必要
	// どちらかがなければFontを正常に構築できないため失敗扱いにする
	if (_pDevice == nullptr || _pRenderer == nullptr) 
	{ return false; }

	// -------------------------------------------------------------------------------
	// フォント及びアトラスの基本設定を保存
	// -------------------------------------------------------------------------------

	// 指定されたフォントサイズをピクセル単位で保持する
	m_FontSizePx	= _fontSizePx;
	// 生成するフォントアトラスのサイズを保存する
	m_AtlasWidth	= _atlasWidth;
	m_AtlasHeight	= _atlasHeight;

	// -------------------------------------------------------------------------------
	// CPU側フォントアトラスをRGBA8で確保
	//
	// outputColor = texColor * vertexColor
	// のようにテクスチャ色と頂点色を乗算して描画する
	// そこでフォントアトラスもRGBA8とし、
	// RGB = 255,255,255 : 白
	// A   = 文字の被覆率 : アンチエイリアスの濃さ
	// とする
	// これなら文字色は頂点カラー側だけで自由に変更でき、
	// 矩形描画と文字描画で同じシェーダーをそのまま使用できる
	// -------------------------------------------------------------------------------
	m_AtlasPixels.assign(static_cast<size_t>(m_AtlasWidth) * m_AtlasHeight * 4, 0);

	// アトラス全体を「白色だが完全透明」の状態で初期化
	// 文字が配置されていない場合はAlpha = 0なので画面には表示されない
	for (size_t i = 0; i < m_AtlasPixels.size(); i += 4)
	{
		m_AtlasPixels[i + 0] = 255;	// R
		m_AtlasPixels[i + 1] = 255;	// G
		m_AtlasPixels[i + 2] = 255;	// B
		m_AtlasPixels[i + 3] = 0;	// A : 初期値は全面透明
	}
	
	// -------------------------------------------------------------------------------
	// DirectWriteFactoryの生成
	// -------------------------------------------------------------------------------

	// DirectWriteの各機能へアクセスする起点となるFactoryを作成する
	// DWRITE_FACTORY_TYPE_SHAREDを指定することで、プロセス内で共有可能なFactoryを使用する
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(m_pDWriteFactory.GetAddressOf()))))
	{
		ELOG("Font::Build() : DWriteCreateFactory failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// Windowsにインストールされているフォント一覧を取得
	// -------------------------------------------------------------------------------
	ComPtr<IDWriteFontCollection> pFontCollection;
	if (FAILED(m_pDWriteFactory->GetSystemFontCollection(pFontCollection.GetAddressOf())))
	{
		ELOG("Font::Build() : GetSystemFontCollection failed");
		return false;
	}
	
	// 指定されたフォントファミリ名がシステムフォント一覧の何番目にあるかを取得
	uint32_t	familyIndex		= 0;		// 何番目にあるかを取得して格納
	BOOL		familyExists	= FALSE;	// 取得が成功か失敗かを取得
	pFontCollection->FindFamilyName(_familyName.c_str(), &familyIndex, &familyExists);
	// 指定フォントがPCになければこのフォントは生成できない
	if (!familyExists)
	{
		ELOG("Font::Build() : font family not found : %ls", _familyName.c_str());
		return false;
	}

	// -------------------------------------------------------------------------------
	// フォントファミリ → フォント → FontFace の順に取得
	// -------------------------------------------------------------------------------

	// familyIndexから目的のフォントファミリを取得する
	// 例 : "Meiryo"というファミリの中にはRegular/Boldなど複数の書体が存在しうる
	ComPtr<IDWriteFontFamily> pFontFamily;
	if (FAILED(pFontCollection->GetFontFamily(familyIndex, pFontFamily.GetAddressOf())))
	{ return false; }

	// 今回は通常の太さ・通常幅・通常スタイルに最も近いフォントを選択する
	ComPtr<IDWriteFont> pFont;
	if (FAILED(pFontFamily->GetFirstMatchingFont(
		DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, pFont.GetAddressOf())))
	{ return false; }

	// FontFaceを作成する
	// FontFaceはグリフ番号取得、メトリクス取得、実際のグリフ描画など、
	// 低レベルな文字処理で使用する中心的なオブジェクト
	if (FAILED(pFont->CreateFontFace(m_pFontFace.GetAddressOf())))
	{ return false; }

	// -------------------------------------------------------------------------------
	// GDI Interopを取得
	// -------------------------------------------------------------------------------
	
	// IDWriteBitmapRenderTargetを使って一度CPU側のビットマップへ文字を描き、
	// そのピクセルを自前のフォントアトラスにコピーする方式
	// CreateBitmapRenderTargetを使用するためにGDIInteropが必要になる
	if (FAILED(m_pDWriteFactory->GetGdiInterop(m_pGdiInterop.GetAddressOf()))) 
	{ return false; }

	// -------------------------------------------------------------------------------
	// 文字を「くっきり」見せるためのラスタライズ設定
	//
	//	ClearTypeLevel = 0	サブピクセル描画を切り、グレースケールのアンチエイリアスにする
	//						そのままだとRGBのにじみ（色の縁取り）がアルファへ混ざる
	//	GDI_CLASSIC			ステム（縦棒）をピクセル境界へ吸着させるヒンティング付きの描画
	//						小さいサイズでも線の太さがそろい、輪郭がぼやけない
	//	EnhancedContrast	細い線が薄く消えるのを防ぐ
	//
	//	既定の CreateRenderingParams はClearType + NATURALなので、
	//	UI用途では輪郭が甘くなる
	// -------------------------------------------------------------------------------
	if (FAILED(m_pDWriteFactory->CreateCustomRenderingParams(
		1.0f,									// ガンマ : 1.0で線形。濃さが素直に出る
		0.7f,									// EnhancedContrast : 細線の消失を抑える
		0.0f,									// ClearTypeLevel : 0でグレースケール
		DWRITE_PIXEL_GEOMETRY_FLAT,				// RGBサブピクセル配置を前提にしない
		DWRITE_RENDERING_MODE_GDI_CLASSIC,		// ピクセル境界を意識した描画
		m_pRenderingParams.GetAddressOf())))
	{
		// カスタム設定が失敗してもFont全体の使用不能にはしない
		// 最低限文字を描画できるように、DirectWrite標準の設定へフォールバックする
		m_pDWriteFactory->CreateRenderingParams(m_pRenderingParams.GetAddressOf());
	}

	// -------------------------------------------------------------------------------
	// フォントメトリクスをpixel単位へ変換
	// -------------------------------------------------------------------------------

	// DirectWriteが保持するフォント固有の寸法情報を取得する
	// ascent / descent / advanceWidth / などはpixelではなくdesignunitで表されている
	DWRITE_FONT_METRICS fontMetrics{};
	m_pFontFace->GetMetrics(&fontMetrics);

	// 「1 design unitが何pixelか」を求める
	// designUnitPerEmがフォント内部の基準サイズ、m_FontSizePxが実際の描画サイズ
	m_UnitsPerEmScale	= m_FontSizePx / static_cast<float>(fontMetrics.designUnitsPerEm);	// m_FontSizePx(16px)
	// 1行を描画するのに必要な高さをpixel単位で求める
	// ascent  : ベースラインより上
	// descent : ベースラインより下
	// lineGap : 行間として推奨される余白
	m_LineHeight		= (fontMetrics.ascent + fontMetrics.descent + fontMetrics.lineGap) * m_UnitsPerEmScale;
	// ベースライン位置を決めるため、ascentだけをpixel単位でも保持する
	m_AscentPx			= fontMetrics.ascent * m_UnitsPerEmScale;

	// -------------------------------------------------------------------------------
	// GPU側フォントアトラステクスチャを生成
	// -------------------------------------------------------------------------------

	// CPU側には既にm_AtlasPixelsがあるため、それと同サイズのGPUTexture2Dを作る
	// 作成直後には空のアトラス内容も一度GPUへアップロードし、
	// EditorUIRendererが参照できるTextureIdも取得する
	if (!CreateAtlasTexture(_pDevice, _pRenderer))
	{
		ELOG("Font::Build() : CreateAtlasTexture failed");
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// グリフ取得
// 
// すでに生成済みならCacheを返し、未生成ならその場でラスタライズしてフォントアトラスへ追加する
// -------------------------------------------------------------------------------
const EditorUI::Glyph* EditorUI::Font::GetGlyph(wchar_t _ch)
{
	// まず、指定された文字がすでに生成済みか確認する
	// 毎フレーム文字をDirectWriteで再生成すると無駄なので
	// 一度作ったGlyphはm_Glyphsにキャッシュして使いまわす
	auto it = m_Glyphs.find(_ch);
	if (it != m_Glyphs.end())
	{
		return &it->second;
	}

	// 未登録の文字だった場合は、新しいGlyph情報を生成する
	Glyph glyph{};

	// DirectWriteで1文字をラスタライズし、CPUフォントアトラスへ配置する
	if (!RasterizeGlyph(_ch, glyph)) 
	{ return nullptr; }	// アトラス満杯、またはラスタライズ失敗

	// 正常に生成できたGlyphをキャッシュへ登録する
	// 以後同じ文字を要求された場合はこの情報をそのまま返せる
	auto [insertedIt, _] = m_Glyphs.emplace(_ch, glyph);
	// unordered_map内に保存されたGlyphのアドレスを返す
	return &insertedIt->second;
}

// -------------------------------------------------------------------------------
// CPU側アトラスの変更をGPUへ反映
// -------------------------------------------------------------------------------
void EditorUI::Font::Flush(RHI::Device* _pDevice)
{
	// RasterizeGlyph()によって新しい文字が追加されている可能性があるため、
	// CPU側のm_AtlasPixelsをGPUテクスチャへアップロードする
	UploadAtlasTexture(_pDevice);
}

// -------------------------------------------------------------------------------
// 1文字をラスタライズしてアトラスに配置
// 
// 処理の流れ
//	1. Unicode文字からDirectWriteのGlyphIndexを取得
//	2. 文字のメトリクスを取得
//	3. 文字1つ分の一時ビットマップを用意
//	4. DirectWriteでそこへ白文字を描画
//	5. 描画結果の輝度をAlphaとしてCPUアトラスへコピー
//	6. UV / Size / Bearing / AdvanceをGlyphに保存
// -------------------------------------------------------------------------------
bool EditorUI::Font::RasterizeGlyph(wchar_t _ch, Glyph& _outGlyph)
{
	// -------------------------------------------------------------------------------
	// Unicodeコードポイント -> DirectWrite Glyph Index
	// -------------------------------------------------------------------------------

	// wchar_t で受け取った文字コードをDirectWriteが要求するUINT32へ変換する
	UINT32 codepoint	= static_cast<UINT32>(_ch);
	// FontFace内部で使用されるグリフ番号を受け取る
	UINT16 glyphIndex	= 0;
	m_pFontFace->GetGlyphIndices(&codepoint, 1, &glyphIndex);

	// -------------------------------------------------------------------------------
	// グリフの寸法情報を取得
	// -------------------------------------------------------------------------------

	// advanceWidthなど、この文字固有のdesignunit単位メトリクスを取得する
	DWRITE_GLYPH_METRICS metrics{};
	m_pFontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &metrics);

	// -------------------------------------------------------------------------------
	// 送り幅(advance)をpixel単位へ変換し、整数へ丸める
	//
	// 小数のまま積み上げると、文字ごとに描画位置が半ピクセルずれ、
	// リニア補間で輪郭がにじむ（これが「ぼやけて見える」いちばんの原因）
	// GDI_CLASSICでラスタライズしている以上、送り幅も整数で扱うのが正しい
	// -------------------------------------------------------------------------------
	const float advancePx = std::round(metrics.advanceWidth * m_UnitsPerEmScale);

	// -------------------------------------------------------------------------------
	// この1文字だけを描画する一時セルの大きさを決定
	// -------------------------------------------------------------------------------

	// このグリフを描画するための、ぴったりサイズのスクラッチ描画面を都度作る
	// 左右にkGlyphOverhang分の余裕を足し、送り幅より広い絵でも切れないようにする
	const int cellWidth		= (std::max)(1, static_cast<int>(advancePx) + (kCellPadding + kGlyphOverhang) * 2);
	const int cellHeight	= (std::max)(1, static_cast<int>(std::ceil(m_LineHeight)) + kCellPadding * 2);

	// -------------------------------------------------------------------------------
	// フォントアトラス内の配置位置を確保
	// -------------------------------------------------------------------------------

	// PlaceInAtlas()が、このセルを置ける左上座標を返す
	// placeX / placeY はCPUアトラス上のpixel座標
	int placeX = 0;
	int placeY = 0;
	if (!PlaceInAtlas(cellWidth, cellHeight, placeX, placeY))
	{
		ELOG("Font::RasterizeGlyph() : atlas full, cannot place glyph U+%04X", static_cast<uint32_t>(_ch));
		return false;
	}

	// -------------------------------------------------------------------------------
	// 1文字専用のスクラッチ描画用を作成
	// -------------------------------------------------------------------------------

	// DirectWriteに直接m_AtlasPixelsへ描画させるのではなく、
	// 一度GDI互換のビットマップへ文字を描き、そのピクセルを後で取り出す
	ComPtr<IDWriteBitmapRenderTarget> pScratch;
	if (FAILED(m_pGdiInterop->CreateBitmapRenderTarget(nullptr, cellWidth, cellHeight, pScratch.GetAddressOf())))
	{
		ELOG("Font::RasterizeGlyph() : CreateBitmapRenderTarget failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// DirectWriteに渡すGlyphRunを構築
	// -------------------------------------------------------------------------------

	DWRITE_GLYPH_RUN run{};
	run.fontFace		= m_pFontFace.Get();	// どのフォントフェイスの文字を描くか指定する
	run.fontEmSize		= m_FontSizePx;			// 描画するフォントサイズを指定
	run.glyphCount		= 1;					// 今回は1文字ずつラスタライズするためGlyph数は1
	run.glyphIndices	= &glyphIndex;			// 描画対象のGlyphIndexを指定する
	FLOAT advance		= 0.0f;					// GlyphRun内での送り量。1文字だけ独立して描画するので、ここでは進める必要がない
	run.glyphAdvances	= &advance;
	DWRITE_GLYPH_OFFSET offset{ 0.0f,0.0f };	// Glyph自体への追加オフセットも今回は使用しない
	run.glyphOffsets	= &offset;

	// -------------------------------------------------------------------------------
	// スクラッチ領域内での描画原点を決定
	// -------------------------------------------------------------------------------

	// X方向は、左側のPadding + Overhang分だけ内側から描画を始める
	// これにより文字形状が左へ多少はみ出してもセル外へ切れにくい
	const float originX = static_cast<float>(kCellPadding + kGlyphOverhang);
	// DirectWriteのY座標は「文字上端」ではなくベースライン位置を基準にする
	// そのため上端PaddingからAscent分だけ下へ進めた位置をベースラインにする
	// Ascentを整数へ丸めることで、文字の上下位置も整数pixelにそろえ、サンプリング時の縦方向のぼやけを抑える
	const float originY = static_cast<float>(kCellPadding) + std::round(m_AscentPx);

	// -------------------------------------------------------------------------------
	// DirectWriteで実際に1文字描画
	// -------------------------------------------------------------------------------

	// ラスタライズモードだけでなく、MeasuringModeもGDI_CLASSICにそろえる
	// 計測方式と描画方式が違うとヒンティング後の寸法と配置にずれが出やすい

	// 描画色は白
	// 後でこの白色の輝度値を文字の被覆率としてAlphaへ変換する
	pScratch->DrawGlyphRun(
		originX, originY, DWRITE_MEASURING_MODE_GDI_CLASSIC, &run,
		m_pRenderingParams.Get(), RGB(255, 255, 255), nullptr);

	// -------------------------------------------------------------------------------
	// DirectWriteの描画結果から生ピクセルへアクセス
	// -------------------------------------------------------------------------------

	// スクラッチのDIBから、R成分(輝度)だけを抜き出してアトラスへコピー
	// IDWriteBitmapRenderTargetが内部で持っているメモリDCを取得
	HDC hMemoryDC = pScratch->GetMemoryDC();
	if (hMemoryDC == nullptr)
	{ return false; }

	// メモリDCに含まれているビットマップを取得
	// このビットマップに先ほどのDrawGlyphRunの中に結果が入っている
	HBITMAP hBitmap = reinterpret_cast<HBITMAP>(GetCurrentObject(hMemoryDC, OBJ_BITMAP));
	if (hBitmap == nullptr) 
	{ return false; }

	// -------------------------------------------------------------------------------
	// DIBSECTIONからビットマップのメモリアドレスと1行のバイト数を取得
	// -------------------------------------------------------------------------------

	// BITMAPだけでなく実ピクセルへのポインタbmBitsを確実に取得できないため、
	// DIBSECTIONとして詳細情報を取得する
	DIBSECTION dibSection{};

	if (GetObject(hBitmap, sizeof(DIBSECTION), &dibSection) == 0) 
	{ return false; }

	// DirectWriteが描いたビットマップの先頭アドレス
	const uint8_t* pSrc = static_cast<const uint8_t*>(dibSection.dsBm.bmBits);
	if (pSrc == nullptr) 
	{ return false; }

	// -------------------------------------------------------------------------------
	// スクラッチの各ピクセルをCPU側フォントアトラスへコピー
	// -------------------------------------------------------------------------------
	for (int y = 0; y < cellHeight; ++y)
	{
		for (int x = 0; x < cellWidth; ++x)
		{
			// スクラッチビットマップ上の(x,y)ピクセル位置を求める
			// 
			// bmWidthBytesは「1行あたり何バイト進むか」を表す
			// 1ピクセルはBGRAの4バイトなのでx * 4 する
			const uint8_t* pixel	= pSrc + (static_cast<size_t>(y) * dibSection.dsBm.bmWidthBytes) + (static_cast<size_t>(x) * 4);
			// スクラッチ内の座標を、アトラス全体の座標へ返還する
			const uint32_t atlasX	= static_cast<uint32_t>(placeX + x);
			const uint32_t atlasY	= static_cast<uint32_t>(placeY + y);

			// RGBA8のm_AtlasPixelsにおける、このピクセルの先頭Indexを求める
			// 1pixel = 4byteなので最後に*4する
			const size_t destIndex = (static_cast<size_t>(atlasY) * m_AtlasWidth + atlasX) * 4;

			// アトラスのRGBは常に白にする
			// 最終的な文字色はEditorUIの頂点カラーとの乗算で決まる
			m_AtlasPixels[destIndex + 0] = 255;
			m_AtlasPixels[destIndex + 1] = 255;
			m_AtlasPixels[destIndex + 2] = 255;
			// DirectWriteのスクラッチはBGRA配列なのでpixel[2]がR
			// 白色で描画した場合、このRの明るさはそのピクセルが文字によってどれだけ覆われているかに相当する
			// その値をAlphaに入れることで、
			// 背景		-> Alpha 0
			// 文字内部 -> Alpha 255
			// 輪郭部分 -> 0～255
			// となり、アンリエイリアス付きの透明文字テクスチャとして使用できる
			m_AtlasPixels[destIndex + 3] = pixel[2];
		}
	}

	// CPU側アトラスが変化したことを記録する
	// 次回Flush時にGPUテクスチャを更新する必要がある
	m_Dirty = true;

	// -------------------------------------------------------------------------------
	// 描画時に必要となるGlyph情報を確定
	// -------------------------------------------------------------------------------

	// Glyph情報を確定させる。UVはピクセル座標→0.0～1.0に正規化する
	// Paddingは隣接グラフとの干渉防止用なので、実際の描画範囲には含めない
	// そのため開始座標では +kCellPadding、サイズでは両端分のPaddingを除外する
	_outGlyph.UV = MakeRect(
		{ static_cast<float>(placeX + kCellPadding) / static_cast<float>(m_AtlasWidth),
		static_cast<float>(placeY + kCellPadding) / static_cast<float>(m_AtlasHeight) },
		{ static_cast<float>(cellWidth - kCellPadding * 2) / static_cast<float>(m_AtlasWidth),
		static_cast<float>(cellHeight - kCellPadding * 2) / static_cast<float>(m_AtlasHeight) });
	_outGlyph.Size = { static_cast<float>(cellWidth - kCellPadding * 2), static_cast<float>(cellHeight - kCellPadding * 2) };

	// -------------------------------------------------------------------------------
	// Bearing
	// -------------------------------------------------------------------------------
	// スクラッチセルの内部では、描画原点をkGlyphOverhang分だけ右に移動させている。
	// そのまま画面へ描画すると文字全体が本来のペン位置より右へずれるため、
	// 描画時には逆方向へkGlyphOverhang戻す必要がある
	// 
	// その補正値をBearing.xとして保存する
	_outGlyph.Bearing = { -static_cast<float>(kGlyphOverhang), 0.0f };

	// この文字を描画した後、次の文字へ進めるpixel数を保存する
	_outGlyph.Advance = advancePx;

	return true;
}

// -------------------------------------------------------------------------------
// アトラス内にグリフセルを配置
// 
// Shelf Packingと呼ばれる単純な方式で、左から右へ順番に並べ、横幅を超えたら次の行へ折り返す
// -------------------------------------------------------------------------------
bool EditorUI::Font::PlaceInAtlas(int _width, int _height, int& _outX, int& _outY)
{
	// m_Cursor.Xは「次のセルを置くX位置」
	// 現在の行に入らなければ次の行へ折り返す
	if (m_Cursor.X + _width > static_cast<int>(m_AtlasWidth))
	{
		m_Cursor.X = 0;						// 新しい行なのでX座標を左端へ戻す
		m_Cursor.Y += m_Cursor.ShelfHeight;	// 現在の行で最も背の高かったセル分だけ下へ進む
		m_Cursor.ShelfHeight = 0;			// 新しい行にはまだ何も配置されていないため、行の高さをリセットする
	}

	// -------------------------------------------------------------------------------
	// 縦方向に空きが残っているか確認
	// -------------------------------------------------------------------------------

	// アトラスの縦方向も使い切っていたらこれ以上置けない
	if (m_Cursor.Y + _height > static_cast<int>(m_AtlasHeight))
	{
		return false;
	}

	// 現在のカーソル位置が、このセルの左上配置位置となる
	_outX = m_Cursor.X;
	_outY = m_Cursor.Y;

	// このセルを置いた分だけ、次の配置位置を右へ進める
	m_Cursor.X += _width;
	// 現在の行で、最も背の高いセルの高さを記録する
	// 次の行へ折り返すとき、この値だけY方向へ進める
	m_Cursor.ShelfHeight = (std::max)(m_Cursor.ShelfHeight, _height);

	return true;
}

// -------------------------------------------------------------------------------
// アトラスGPUテクスチャの初回生成
// 
// CPU側のm_AtlasPixelsと同サイズ・同フォーマットのTexture2Dを作成し、EditorUIRendererから参照できるよう登録
// -------------------------------------------------------------------------------
bool EditorUI::Font::CreateAtlasTexture(RHI::Device* _pDevice, EditorUIRenderer* _pRenderer)
{
	auto* pDevice = _pDevice->GetDevice();

	// -------------------------------------------------------------------------------
	// GPUローカルのDEFAULT Heapを指定
	// -------------------------------------------------------------------------------

	// 最終的なフォントアトラスはPixelShaderから頻繁に読み込まれるため
	// CPUアクセス可能なUPLOAD Heapではなく、GPUアクセスに適したDEFAULT Heapへ置く
	D3D12_HEAP_PROPERTIES heapDefault{};
	heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

	// Texture2Dのリソース情報を設定
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width				= m_AtlasWidth;
	resDesc.Height				= m_AtlasHeight;
	resDesc.DepthOrArraySize	= 1;
	resDesc.MipLevels			= 1;
	resDesc.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;	// 白 + 被覆率(アルファ)
	resDesc.SampleDesc.Count	= 1;
	resDesc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;

	// GPUリソースを作成
	// COPY_DESTにすることで、CPU側のm_AtlasPixelsの内容をUploadBuffer経由でコピーする
	ComPtr<ID3D12Resource> pResource;
	if (FAILED(pDevice->CreateCommittedResource(
		&heapDefault, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(pResource.GetAddressOf()))))
	{
		ELOG("Font::CreateAtlasTexture() : CreateCommittedResource failed");
		return false;
	}

	// Textureオブジェクトに先にSRVを作らせておく
	if (!m_AtlasTextureObj.InitFromResource(
		pDevice, _pDevice->GetPool(RHI::Device::POOL_TYPE_RES), pResource.Get(), DXGI_FORMAT_R8G8B8A8_UNORM))
	{
		ELOG("Font::CreateAtlasTexture() : Texture::InitFromResource failed");
		return false;
	}

	// -------------------------------------------------------------------------------
	// 初期アトラスを一度GPUへ転送
	// -------------------------------------------------------------------------------
	
	// GPUテクスチャ作成直後は中身が未定義なので
	// CPU側で初期化した「白RGB + Alpha0」のアトラスをアップロードする
	m_Dirty = true;
	UploadAtlasTexture(_pDevice);

	// -------------------------------------------------------------------------------
	// EditorUIRendererにテクスチャを登録
	// -------------------------------------------------------------------------------

	// DrawCommand等から直接Textureオブジェクトのポインタを扱わず、
	// Rendererが管理するTextureIdを通して参照できるようにする
	m_TextureId = _pRenderer->RegisterTexture(&m_AtlasTextureObj);
	return true;
}

// -------------------------------------------------------------------------------
// CPU側フォントアトラスをGPUテクスチャへアップロード
// 
// RasterizeGlyph()によって新しい文字が増えた場合、m_AtlasPixelsの内容をGPU側のTexture2Dへコピーする
// この処理ではDeviceが普段使用している共有コマンドリストを触らず、一時的なものを使用
// -------------------------------------------------------------------------------
void EditorUI::Font::UploadAtlasTexture(RHI::Device* _pDevice)
{
	// -------------------------------------------------------------------------------
	// 更新不要なら何もしない
	// -------------------------------------------------------------------------------

	// RasterizeGlyph()が新しい文字を追加するとm_Dirty = trueになる
	if (!m_Dirty)
	{ return; }

	// デバイス取得
	auto* pDevice = _pDevice->GetDevice();
	// 転送先となるGPU側フォントアトラスのID3D12Resourceを取得する
	ID3D12Resource* pAtlasResource = m_AtlasTextureObj.GetResource();

	// -------------------------------------------------------------------------------
	// アップロードバッファに必要なサイズを計算
	// -------------------------------------------------------------------------------

	// Texture2Dは行ピッチのアライメントなどがあるため、
	// GetRequiredIntermediateSize()でUpdateSubresourceに必要な容量を取得する
	const UINT64 uploadSize = GetRequiredIntermediateSize(pAtlasResource, 0, 1);
	// CPUから書き込めるUPLOAD Heapを使用
	D3D12_HEAP_PROPERTIES heapUploads{};
	heapUploads.Type = D3D12_HEAP_TYPE_UPLOAD;

	// -------------------------------------------------------------------------------
	// 一時アップロードバッファを作成
	// -------------------------------------------------------------------------------

	// Textureへ直接CPUmemcpyはできないため、
	// CPU->UploadBuffer->DEFAULT Heap Textureの順でコピーする
	D3D12_RESOURCE_DESC uploadDesc{};
	uploadDesc.Dimension		= D3D12_RESOURCE_DIMENSION_BUFFER;	// Upload用の中間リソースはBufferとして作る
	uploadDesc.Width			= uploadSize;
	uploadDesc.Height			= 1;
	uploadDesc.DepthOrArraySize = 1;
	uploadDesc.MipLevels		= 1;
	uploadDesc.Format			= DXGI_FORMAT_UNKNOWN;
	uploadDesc.SampleDesc.Count = 1;
	uploadDesc.Layout			= D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ComPtr<ID3D12Resource> pUpload;
	if (FAILED(pDevice->CreateCommittedResource(
		&heapUploads, D3D12_HEAP_FLAG_NONE, &uploadDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(pUpload.GetAddressOf()))))
	{
		ELOG("Font::UploadAtlasTexture() : upload buffer creation failed");
		return;
	}

	// -------------------------------------------------------------------------------
	// CPU側アトラスのメモリ構造をD3D12_SUBRESOURCE_DATAとして説明
	// -------------------------------------------------------------------------------

	D3D12_SUBRESOURCE_DATA sub{};
	sub.pData		= m_AtlasPixels.data();	// コピー元データの先頭
	sub.RowPitch	= static_cast<LONG_PTR>(m_AtlasWidth) * 4;	// R8G8B8A8_UNORM : 4バイト/ピクセル
	sub.SlicePitch	= static_cast<LONG_PTR>(m_AtlasPixels.size());

	// -------------------------------------------------------------------------------
	// このアップロード専用の一時コマンド環境を作成
	// -------------------------------------------------------------------------------

	// 共有のDevice::m_CommandListには触れず、この関数専用の
	// 使い捨てコマンドアロケータ / リスト / フェンスで完結させる
	ComPtr<ID3D12CommandAllocator>		pAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pCmd;
	ComPtr<ID3D12Fence>					pFence;

	// DIRECTタイプのCommandAllocatorを生成する
	if (FAILED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(pAlloc.GetAddressOf())))) 
	{ return; }
	// 上で作ったAllocatorを使用するCommandListを生成する
	if (FAILED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAlloc.Get(), nullptr, IID_PPV_ARGS(pCmd.GetAddressOf())))) 
	{ return; }

	// -------------------------------------------------------------------------------
	// 2回目以降のアップロードではSRV状態->COPY_DESTへ戻す
	// -------------------------------------------------------------------------------
	
	// 初回アップロード : TextureをCOPY_DEST状態として作った直後なので、そのままコピーできる
	// 2回目以降        : 前回アップロードの最後でPIXEL_SHADER_RESOURCEへ遷移しているのでコピー前にCOPY_DESTへ戻す
	D3D12_RESOURCE_BARRIER toCopyDest{};
	toCopyDest.Type						= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopyDest.Transition.pResource		= pAtlasResource;
	toCopyDest.Transition.StateBefore	= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	toCopyDest.Transition.StateAfter	= D3D12_RESOURCE_STATE_COPY_DEST;
	toCopyDest.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// CreateAtlasTexture()では、最初のUploadAtlasTexture()を呼んだ後で
	// RegisterTexture()を行ってm_TextureIdを設定している
	//
	// そのため、m_TextureId == 0はRegisterTexture前 == 初回アップロードと判断する目印として活用
	const bool isFirstUpload = (m_TextureId == 0);
	if (!isFirstUpload)
	{
		pCmd->ResourceBarrier(1, &toCopyDest);
	}

	// -------------------------------------------------------------------------------
	// CPUデータ->UploadBuffer->GPUTextureへコピー
	// -------------------------------------------------------------------------------

	// UpdateSubresourceが内部でUploadBufferへのデータ配置とCopyTextureRegion相当のコマンド記録をまとめて行う
	UpdateSubresources(pCmd.Get(), pAtlasResource, pUpload.Get(), 0, 0, 1, &sub);
	
	// -------------------------------------------------------------------------------
	// COPY_DEST->PIXEL_SHADER_RESOURCEへ遷移
	// -------------------------------------------------------------------------------

	// コピー後はこのテクスチャをPixelShaderから文字描画用SRVとして読むため
	// PIXEL_SHADER_RESOURCE状態へ変更しておく必要がある
	D3D12_RESOURCE_BARRIER toShaderResource{};
	toShaderResource.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toShaderResource.Transition.pResource	= pAtlasResource;
	toShaderResource.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	toShaderResource.Transition.StateAfter	= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	toShaderResource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	pCmd->ResourceBarrier(1, &toShaderResource);

	// コマンド記録を終了する
	pCmd->Close();

	// -------------------------------------------------------------------------------
	// GPUへコマンドリストを送信
	// -------------------------------------------------------------------------------

	ID3D12CommandList* ppLists[] = { pCmd.Get() };
	_pDevice->GetQueue()->ExecuteCommandLists(1, ppLists);

	// -------------------------------------------------------------------------------
	// 転送完了待ち用Fenceを作成
	// -------------------------------------------------------------------------------

	// pUploadやpCmdはこの関数を抜けると破棄される
	// GPUがまだそれらを使用している途中で破棄してはいけないので、Fenceを使って今回のコピーが完了するまでCPUで待機
	if (FAILED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(pFence.GetAddressOf()))))
	{
		return;
	}

	// Fence完了を待つためのWindowEventを作成する
	HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!hEvent) { return; }

	_pDevice->GetQueue()->Signal(pFence.Get(), 1);	// CommandQueueにFence値1をSignalする
	pFence->SetEventOnCompletion(1, hEvent);		// Fenceが1へ到達したとき、hEventをシグナル状態になる用登録
	WaitForSingleObject(hEvent, INFINITE);			// GPUコピー完了まで実際に待つ
	CloseHandle(hEvent);							// WindowsEventHandleを解除

	m_Dirty = false;

}
