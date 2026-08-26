// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ViewportPanel.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Engine/Renderer/ViewportTarget/ViewportTarget.h>

Editor::ViewportPanel::ViewportPanel()
{
	// 中央に大きく開く。ドッキングされるまでの初期表示
	SetInitialPlacement({ 320.0f, 60.0f }, { 640.0f, 400.0f });
}

void Editor::ViewportPanel::BeginFrame()
{
	m_RequestedWidth	= 0;
	m_RequestedHeight	= 0;
	m_VisibleThisFrame	= false;
	m_ViewportHovered	= false;
}

// -------------------------------------------------------------------------------
// ゲーム画面の表示
//
// 処理の流れ
//	1. パネルの表示可能領域を測り、必要な描画サイズとして記録する
//	2. すでに描かれているテクスチャがあれば、その領域いっぱいに貼る
// -------------------------------------------------------------------------------
void Editor::ViewportPanel::OnGUI(EditorContext& _ctx)
{
	EditorUI::Context&		ui		= *_ctx.pUI;
	EditorUI::WindowFrame*	pFrame	= ui.GetCurrentWindow();

	if (pFrame == nullptr)
	{ return; }

	// -------------------------------------------------------------------------------
	// 表示領域を求める
	//
	// ゲーム画面は余白なしで敷き詰めたいので、
	// 他のウィジェットのようなパディングは入れずコンテンツ矩形をそのまま使う
	// -------------------------------------------------------------------------------
	const EditorUI::Rect2D& contentRect = pFrame->ContentRect;

	const float width  = contentRect.Width();
	const float height = contentRect.Height();

	if (width < 1.0f || height < 1.0f)
	{
		return;	// 潰れている。描く意味がない
	}

	m_RequestedWidth	= static_cast<uint32_t>(width);
	m_RequestedHeight	= static_cast<uint32_t>(height);
	m_VisibleThisFrame	= true;

	// -------------------------------------------------------------------------------
	// カメラ操作を許す範囲を決める
	//
	//	ゲーム画面の内側であっても、次の場合はカメラへ渡してはいけない
	//		1. UIが何らかのポインタ操作中（ウィジェット編集・ウィンドウ移動など）
	//		2. フローティング時の右下 : リサイズグリップと重なる
	//
	//	2を除外しないと、グリップを掴んだ瞬間にカメラが起動して
	//	カーソルが画面中央へ固定され、ウィンドウの大きさが変えられなくなる
	// -------------------------------------------------------------------------------
	EditorUI::Rect2D cameraZone = contentRect;

	if (!pFrame->IsDocked)
	{
		const float gripSize = ui.GetStyle().ResizeGripSize;
		cameraZone.Max.x -= gripSize;
		cameraZone.Max.y -= gripSize;
	}

	m_ViewportHovered =
		ui.IsCurrentWindowHovered() &&
		!ui.IsUiOperationActive() &&
		cameraZone.IsValid() &&
		cameraZone.Contains(ui.GetMousePos());

	// -------------------------------------------------------------------------------
	// 描かれたテクスチャを貼る
	// -------------------------------------------------------------------------------
	if (_ctx.pViewport == nullptr || !_ctx.pViewport->IsValid())
	{
		// 初回フレームなど、まだレンダーターゲットが用意できていない場合
		EditorUI::TextMuted(ui, *_ctx.pFont, "Preparing render target...");
		return;
	}

	// -------------------------------------------------------------------------------
	// レンダーターゲットの大きさとパネルの大きさが一致しない瞬間がある
	// （リサイズ要求は次のフレームの描画に反映されるため）
	// そのときも絵が途切れないよう、テクスチャ側の実サイズに合わせてUVを取る
	// -------------------------------------------------------------------------------
	const float targetWidth  = static_cast<float>(_ctx.pViewport->GetWidth());
	const float targetHeight = static_cast<float>(_ctx.pViewport->GetHeight());

	EditorUI::Rect2D uv{ { 0.0f, 0.0f }, { 1.0f, 1.0f } };

	if (targetWidth > 0.0f && targetHeight > 0.0f)
	{
		uv.Max.x = (std::min)(1.0f, width  / targetWidth);
		uv.Max.y = (std::min)(1.0f, height / targetHeight);
	}

	// コンテンツ矩形の左上へ直接貼るため、レイアウトのカーソルは使わない
	pFrame->Draw.AddImage(contentRect, _ctx.pViewport->GetTextureId(), uv);
}
