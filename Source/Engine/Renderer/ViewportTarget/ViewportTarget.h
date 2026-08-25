#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Types.h>
#include <Engine/Renderer/SceneOutput/SceneOutput.h>

namespace RHI
{
	class Device;
	class ColorTarget;
	class DepthTarget;
	class Texture;
}

class EditorUIRenderer;

// -------------------------------------------------------------------------------
// ViewportTarget class
//
// 概要 :
//	ゲーム画面を「バックバッファではなく、テクスチャへ」描くためのレンダーターゲット
//
//	これがあることで、ゲーム画面はエディタ上のただの1枚の絵になり、
//	他のパネルとまったく同じようにドッキング・リサイズできるようになる
//
// 1フレームの流れ :
//	Resize(パネルの大きさ)      パネルの大きさが変わったときだけ作り直す
//	Begin(cmd)                  RENDER_TARGET へ遷移 → クリア → RTV/DSV/Viewport設定
//	SceneManager::Render(cmd)   シーンはここへ描かれる
//	End(cmd)                    PIXEL_SHADER_RESOURCE へ遷移
//	GetTextureId()              EditorUIのImageウィジェットへ渡す
//
// 責務の分担 :
//	ViewportTarget	オフスクリーンの色/深度バッファとSRVの管理
//	Renderer		バックバッファに対するフレーム制御（従来どおり）
//	ViewportPanel	描いた結果をどこにどう表示するか
// -------------------------------------------------------------------------------
class ViewportTarget
{
public:

	ViewportTarget();
	~ViewportTarget();

	// -------------------------------------------------------------------------------
	// @brief	初期化。まだGPUリソースは作らず、最初のResizeで確保する
	//
	// @param[in]	_pDevice	デバイス
	// @param[in]	_pRenderer	SRVをEditorUIへ登録するためのレンダラ
	// -------------------------------------------------------------------------------
	bool Init(RHI::Device* _pDevice, EditorUIRenderer* _pRenderer);

	void Term();

	// -------------------------------------------------------------------------------
	// @brief	要求サイズに合わせてリソースを作り直す
	//
	//	同じサイズなら何もしないため、毎フレーム呼んで構わない
	//	作り直しの際はGPUの完了を待つ。描画中のリソースを解放しないため
	//
	// @param[in]	_width	要求する横幅(ピクセル)
	// @param[in]	_height	要求する縦幅(ピクセル)
	// @retval	true	この呼び出しで使える状態になっている
	// -------------------------------------------------------------------------------
	bool Resize(uint32_t _width, uint32_t _height);

	// -------------------------------------------------------------------------------
	// @brief	このターゲットへの描画を開始する
	//
	//	バックバッファ用の設定を上書きするため、Endのあとに描画先を戻す必要がある
	// -------------------------------------------------------------------------------
	void Begin(ID3D12GraphicsCommandList* _pCmd);

	// -------------------------------------------------------------------------------
	// @brief	描画を終了し、テクスチャとして読める状態へ遷移させる
	// -------------------------------------------------------------------------------
	void End(ID3D12GraphicsCommandList* _pCmd);

	// -------------------------------------------------------------------------------
	// @brief	シーンの描画先としてこのターゲットを表す指定を返す
	//
	//	SceneRendererはこれを受け取って、バックバッファの代わりにここへ描く
	//	リソースが未確保のときは無効なSceneOutputを返す
	// -------------------------------------------------------------------------------
	SceneOutput GetSceneOutput() const;

	// -------------------------------------------------------------------------------
	// @brief	EditorUIのImageウィジェットへ渡すテクスチャId
	// -------------------------------------------------------------------------------
	EditorUI::TextureId GetTextureId() const;

	uint32_t	GetWidth()	const { return m_Width; }
	uint32_t	GetHeight() const { return m_Height; }

	// 縦横比。カメラのアスペクトをパネルに合わせるために使う
	float		GetAspect() const;

	// 描画可能なリソースを持っているか
	bool		IsValid()	const;

private:

	// 現在のリソースを解放する。GPU完了待ちは呼び出し側の責任
	void ReleaseResources();

	RHI::Device*		m_pDevice	= nullptr;	// 所有権なし
	EditorUIRenderer*	m_pRenderer	= nullptr;	// 所有権なし

	std::unique_ptr<RHI::ColorTarget>	m_pColor;
	std::unique_ptr<RHI::DepthTarget>	m_pDepth;
	std::unique_ptr<RHI::Texture>		m_pTexture;	// 色バッファをSRVとして読むためのビュー

	EditorUI::TextureId m_TextureId = 0;

	uint32_t m_Width	= 0;
	uint32_t m_Height	= 0;

	// 背景色。ゲーム画面とエディタの余白を区別できる程度に暗くしておく
	static constexpr float kClearColor[4] = { 0.06f, 0.07f, 0.09f, 1.0f };

	ViewportTarget	(const ViewportTarget&) = delete;
	void operator = (const ViewportTarget&) = delete;
};
