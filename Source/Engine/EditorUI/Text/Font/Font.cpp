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

bool EditorUI::Font::Build(
	const std::wstring& _familyName,
	float				_fontSizePx,
	RHI::Device*		_pDevice, 
	EditorUIRenderer*	_pRenderer,
	uint32_t			_atlasWidth, 
	uint32_t			_atlasHeight)
{
	if (_pDevice == nullptr || _pRenderer == nullptr) 
	{ return false; }

	m_FontSizePx	= _fontSizePx;
	m_AtlasWidth	= _atlasWidth;
	m_AtlasHeight	= _atlasHeight;
	// -------------------------------------------------------------------------------
	// アトラスはRGBA8で持つ
	//
	// 1チャンネル(R8)にすると、シェーダーのtexColor * vertexColorで
	// GとBが0になり、文字が赤くなったうえ背景が不透明の黒で塗り潰されてしまう
	// RGBを白で埋め、被覆率(アンチエイリアスの濃さ)をAへ入れることで、
	// 矩形と文字をまったく同じシェーダーで扱える
	// -------------------------------------------------------------------------------
	m_AtlasPixels.assign(static_cast<size_t>(m_AtlasWidth) * m_AtlasHeight * 4, 0);

	for (size_t i = 0; i < m_AtlasPixels.size(); i += 4)
	{
		m_AtlasPixels[i + 0] = 255;	// R
		m_AtlasPixels[i + 1] = 255;	// G
		m_AtlasPixels[i + 2] = 255;	// B
		m_AtlasPixels[i + 3] = 0;	// A : 初期値は全面透明
	}
	
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(m_pDWriteFactory.GetAddressOf()))))
	{
		ELOG("Font::Build() : DWriteCreateFactory failed");
		return false;
	}

	ComPtr<IDWriteFontCollection> pFontCollection;
	if (FAILED(m_pDWriteFactory->GetSystemFontCollection(pFontCollection.GetAddressOf())))
	{
		ELOG("Font::Build() : GetSystemFontCollection failed");
		return false;
	}
	
	uint32_t familyIndex = 0;
	BOOL familyExists = FALSE;
	pFontCollection->FindFamilyName(_familyName.c_str(), &familyIndex, &familyExists);
	if (!familyExists)
	{
		ELOG("Font::Build() : font family not found : %ls", _familyName.c_str());
		return false;
	}

	ComPtr<IDWriteFontFamily> pFontFamily;
	if (FAILED(pFontCollection->GetFontFamily(familyIndex, pFontFamily.GetAddressOf())))
	{ return false; }

	ComPtr<IDWriteFont> pFont;
	if (FAILED(pFontFamily->GetFirstMatchingFont(
		DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, pFont.GetAddressOf())))
	{ return false; }

	if (FAILED(pFont->CreateFontFace(m_pFontFace.GetAddressOf())))
	{ return false; }

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
		DWRITE_PIXEL_GEOMETRY_FLAT,
		DWRITE_RENDERING_MODE_GDI_CLASSIC,
		m_pRenderingParams.GetAddressOf())))
	{
		// 失敗しても描画自体は続けられるよう、既定のパラメータへ落とす
		m_pDWriteFactory->CreateRenderingParams(m_pRenderingParams.GetAddressOf());
	}

	DWRITE_FONT_METRICS fontMetrics{};
	m_pFontFace->GetMetrics(&fontMetrics);
	m_UnitsPerEmScale	= m_FontSizePx / static_cast<float>(fontMetrics.designUnitsPerEm);
	m_LineHeight		= (fontMetrics.ascent + fontMetrics.descent + fontMetrics.lineGap) * m_UnitsPerEmScale;
	m_AscentPx			= fontMetrics.ascent * m_UnitsPerEmScale;

	if (!CreateAtlasTexture(_pDevice, _pRenderer))
	{
		ELOG("Font::Build() : CreateAtlasTexture failed");
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------
// グリフ取得
// -------------------------------------------------------------------------------
const EditorUI::Glyph* EditorUI::Font::GetGlyph(wchar_t _ch)
{
	auto it = m_Glyphs.find(_ch);
	if (it != m_Glyphs.end())
	{
		return &it->second;
	}

	Glyph glyph{};
	if (!RasterizeGlyph(_ch, glyph)) 
	{ return nullptr; }	// アトラス満杯、またはラスタライズ失敗

	auto [insertedIt, _] = m_Glyphs.emplace(_ch, glyph);
	return &insertedIt->second;
}

void EditorUI::Font::Flush(RHI::Device* _pDevice)
{
	UploadAtlasTexture(_pDevice);
}

// -------------------------------------------------------------------------------
// 1文字をラスタライズしてアトラスに配置
// -------------------------------------------------------------------------------
bool EditorUI::Font::RasterizeGlyph(wchar_t _ch, Glyph& _outGlyph)
{
	UINT32 codepoint	= static_cast<UINT32>(_ch);
	UINT16 glyphIndex	= 0;
	m_pFontFace->GetGlyphIndices(&codepoint, 1, &glyphIndex);

	DWRITE_GLYPH_METRICS metrics{};
	m_pFontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &metrics);

	// -------------------------------------------------------------------------------
	// 送り幅は整数へ丸める
	//
	// 小数のまま積み上げると、文字ごとに描画位置が半ピクセルずれ、
	// リニア補間で輪郭がにじむ（これが「ぼやけて見える」いちばんの原因）
	// GDI_CLASSICでラスタライズしている以上、送り幅も整数で扱うのが正しい
	// -------------------------------------------------------------------------------
	const float advancePx = std::round(metrics.advanceWidth * m_UnitsPerEmScale);

	// このグリフを描画するための、ぴったりサイズのスクラッチ描画面を都度作る
	// 左右にkGlyphOverhang分の余裕を足し、送り幅より広い絵でも切れないようにする
	const int cellWidth		= (std::max)(1, static_cast<int>(advancePx) + (kCellPadding + kGlyphOverhang) * 2);
	const int cellHeight	= (std::max)(1, static_cast<int>(std::ceil(m_LineHeight)) + kCellPadding * 2);

	int placeX = 0;
	int placeY = 0;
	if (!PlaceInAtlas(cellWidth, cellHeight, placeX, placeY))
	{
		ELOG("Font::RasterizeGlyph() : atlas full, cannot place glyph U+%04X", static_cast<uint32_t>(_ch));
		return false;
	}

	ComPtr<IDWriteBitmapRenderTarget> pScratch;
	if (FAILED(m_pGdiInterop->CreateBitmapRenderTarget(nullptr, cellWidth, cellHeight, pScratch.GetAddressOf())))
	{
		ELOG("Font::RasterizeGlyph() : CreateBitmapRenderTarget failed");
		return false;
	}

	DWRITE_GLYPH_RUN run{};
	run.fontFace		= m_pFontFace.Get();
	run.fontEmSize		= m_FontSizePx;
	run.glyphCount		= 1;
	run.glyphIndices	= &glyphIndex;
	FLOAT advance		= 0.0f;
	run.glyphAdvances	= &advance;
	DWRITE_GLYPH_OFFSET offset{ 0.0f,0.0f };
	run.glyphOffsets	= &offset;

	// ベースラインは上端からascent分だけ下がった位置
	// ベースラインを整数へ丸めることで、行ごとの上下のにじみも防ぐ
	const float originX = static_cast<float>(kCellPadding + kGlyphOverhang);
	const float originY = static_cast<float>(kCellPadding) + std::round(m_AscentPx);

	// 計測モードもGDI_CLASSICにそろえる
	// ラスタライズと計測がずれると、ヒンティングの意味がなくなる
	pScratch->DrawGlyphRun(
		originX, originY, DWRITE_MEASURING_MODE_GDI_CLASSIC, &run,
		m_pRenderingParams.Get(), RGB(255, 255, 255), nullptr);

	// スクラッチのDIBから、R成分(輝度)だけを抜き出してアトラスへコピー
	// IDWriteBitmapRenderTargetが内部で持っているメモリDCを取得
	HDC hMemoryDC = pScratch->GetMemoryDC();
	if (hMemoryDC == nullptr)
	{ return false; }

	// メモリDCに含まれているビットマップを取得
	HBITMAP hBitmap = reinterpret_cast<HBITMAP>(GetCurrentObject(hMemoryDC, OBJ_BITMAP));
	if (hBitmap == nullptr) 
	{ return false; }

	// bmBitssまで取得するためBITMAPではなくDIBSECTIONを使う
	DIBSECTION dibSection{};

	if (GetObject(hBitmap, sizeof(DIBSECTION), &dibSection) == 0) 
	{ return false; }
	const uint8_t* pSrc = static_cast<const uint8_t*>(dibSection.dsBm.bmBits);
	if (pSrc == nullptr) 
	{ return false; }

	for (int y = 0; y < cellHeight; ++y)
	{
		for (int x = 0; x < cellWidth; ++x)
		{
			const uint8_t* pixel	= pSrc + (static_cast<size_t>(y) * dibSection.dsBm.bmWidthBytes) + (static_cast<size_t>(x) * 4);
			const uint32_t atlasX	= static_cast<uint32_t>(placeX + x);
			const uint32_t atlasY	= static_cast<uint32_t>(placeY + y);

			// DirectWriteは白文字として描くため、BGRAのR成分がそのまま被覆率になる
			// それをアルファへ入れ、色は頂点カラー側で決められるようにする
			const size_t destIndex = (static_cast<size_t>(atlasY) * m_AtlasWidth + atlasX) * 4;

			m_AtlasPixels[destIndex + 0] = 255;
			m_AtlasPixels[destIndex + 1] = 255;
			m_AtlasPixels[destIndex + 2] = 255;
			m_AtlasPixels[destIndex + 3] = pixel[2];
		}
	}
	m_Dirty = true;

	// Glyph情報を確定させる。UVはピクセル座標→0.0～1.0に正規化する
	_outGlyph.UV = MakeRect(
		{ static_cast<float>(placeX + kCellPadding) / static_cast<float>(m_AtlasWidth),
		static_cast<float>(placeY + kCellPadding) / static_cast<float>(m_AtlasHeight) },
		{ static_cast<float>(cellWidth - kCellPadding * 2) / static_cast<float>(m_AtlasWidth),
		static_cast<float>(cellHeight - kCellPadding * 2) / static_cast<float>(m_AtlasHeight) });
	_outGlyph.Size = { static_cast<float>(cellWidth - kCellPadding * 2), static_cast<float>(cellHeight - kCellPadding * 2) };
	// セル左端はペン位置よりkGlyphOverhangだけ左にある
	// その分を戻さないと、文字全体が右へずれてしまう
	_outGlyph.Bearing = { -static_cast<float>(kGlyphOverhang), 0.0f };
	_outGlyph.Advance = advancePx;

	return true;
}

bool EditorUI::Font::PlaceInAtlas(int _width, int _height, int& _outX, int& _outY)
{
	// 現在の行に入らなければ次の行へ折り返す
	if (m_Cursor.X + _width > static_cast<int>(m_AtlasWidth))
	{
		m_Cursor.X = 0;
		m_Cursor.Y += m_Cursor.ShelfHeight;
		m_Cursor.ShelfHeight = 0;
	}

	// アトラスの縦方向も使い切っていたらこれ以上置けない
	if (m_Cursor.Y + _height > static_cast<int>(m_AtlasHeight))
	{
		return false;
	}

	_outX = m_Cursor.X;
	_outY = m_Cursor.Y;

	m_Cursor.X += _width;
	m_Cursor.ShelfHeight = (std::max)(m_Cursor.ShelfHeight, _height);

	return true;
}

// -------------------------------------------------------------------------------
// アトラスGPUテクスチャの初回生成
// -------------------------------------------------------------------------------
bool EditorUI::Font::CreateAtlasTexture(RHI::Device* _pDevice, EditorUIRenderer* _pRenderer)
{
	auto* pDevice = _pDevice->GetDevice();

	D3D12_HEAP_PROPERTIES heapDefault{};
	heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Dimension			= D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Width				= m_AtlasWidth;
	resDesc.Height				= m_AtlasHeight;
	resDesc.DepthOrArraySize	= 1;
	resDesc.MipLevels			= 1;
	resDesc.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;	// 白 + 被覆率(アルファ)
	resDesc.SampleDesc.Count	= 1;
	resDesc.Layout				= D3D12_TEXTURE_LAYOUT_UNKNOWN;

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

	m_Dirty = true;
	UploadAtlasTexture(_pDevice);

	m_TextureId = _pRenderer->RegisterTexture(&m_AtlasTextureObj);
	return true;
}

void EditorUI::Font::UploadAtlasTexture(RHI::Device* _pDevice)
{
	if (!m_Dirty)
	{ return; }

	auto* pDevice = _pDevice->GetDevice();
	ID3D12Resource* pAtlasResource = m_AtlasTextureObj.GetResource();

	const UINT64 uploadSize = GetRequiredIntermediateSize(pAtlasResource, 0, 1);
	D3D12_HEAP_PROPERTIES heapUploads{};
	heapUploads.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC uploadDesc{};
	uploadDesc.Dimension		= D3D12_RESOURCE_DIMENSION_BUFFER;
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

	D3D12_SUBRESOURCE_DATA sub{};
	sub.pData		= m_AtlasPixels.data();
	sub.RowPitch	= static_cast<LONG_PTR>(m_AtlasWidth) * 4;	// R8G8B8A8_UNORM : 4バイト/ピクセル
	sub.SlicePitch	= static_cast<LONG_PTR>(m_AtlasPixels.size());

	// 共有のDevice::m_CommandListには触れず、この関数専用の
	// 使い捨てコマンドアロケータ / リスト / フェンスで完結させる
	ComPtr<ID3D12CommandAllocator>		pAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pCmd;
	ComPtr<ID3D12Fence>					pFence;

	if (FAILED(pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(pAlloc.GetAddressOf())))) 
	{ return; }
	if (FAILED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pAlloc.Get(), nullptr, IID_PPV_ARGS(pCmd.GetAddressOf())))) 
	{ return; }

	// ２回目以降のFlushでは、直前のPIXEL_SHADER_RESOURCE状態のはずなので、COPY_DESTへ戻す
	// 初回はCOPY_DESTのままなので、このバリアは不要
	D3D12_RESOURCE_BARRIER toCopyDest{};
	toCopyDest.Type						= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toCopyDest.Transition.pResource		= pAtlasResource;
	toCopyDest.Transition.StateBefore	= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	toCopyDest.Transition.StateAfter	= D3D12_RESOURCE_STATE_COPY_DEST;
	toCopyDest.Transition.Subresource	= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	const bool isFirstUpload = (m_TextureId == 0); // RegisterTexture前=初回呼び出しの目印
	if (!isFirstUpload)
	{
		pCmd->ResourceBarrier(1, &toCopyDest);
	}

	UpdateSubresources(pCmd.Get(), pAtlasResource, pUpload.Get(), 0, 0, 1, &sub);

	D3D12_RESOURCE_BARRIER toShaderResource{};
	toShaderResource.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	toShaderResource.Transition.pResource	= pAtlasResource;
	toShaderResource.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	toShaderResource.Transition.StateAfter	= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	toShaderResource.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	pCmd->ResourceBarrier(1, &toShaderResource);

	pCmd->Close();

	ID3D12CommandList* ppLists[] = { pCmd.Get() };
	_pDevice->GetQueue()->ExecuteCommandLists(1, ppLists);

	if (FAILED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(pFence.GetAddressOf()))))
	{
		return;
	}

	HANDLE hEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!hEvent) { return; }

	_pDevice->GetQueue()->Signal(pFence.Get(), 1);
	pFence->SetEventOnCompletion(1, hEvent);
	WaitForSingleObject(hEvent, INFINITE);
	CloseHandle(hEvent);

	m_Dirty = false;

}
