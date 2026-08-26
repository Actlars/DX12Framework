// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ViewportTarget.h"

#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/RHI/Resource/Target/ColorTarget/ColorTarget.h>
#include <Engine/RHI/Resource/Target/DepthTarget/DepthTarget.h>
#include <Engine/RHI/Resource/Texture/Texture.h>
#include <Engine/RHI/Resource/ResourceStateTracker/ResourceStateTracker.h>
#include <Engine/EditorUI/Render/EditorUIRenderer/EditorUIRenderer.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

namespace
{
	// バックバッファと同じ形式にしておくと、色の見え方がそのまま一致する
	constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;

	// 極端に小さい・大きいサイズを弾くための上下限
	constexpr uint32_t kMinSize = 64;
	constexpr uint32_t kMaxSize = 8192;

	// -------------------------------------------------------------------------------
	// 確保サイズの粒度
	//
	//	リサイズのたびにGPUリソースを作り直すと、ドラッグ中は毎フレーム
	//	「GPU完了待ち → 破棄 → 再生成」が走り、動きが固まって見える
	//	また、ディスクリプタの確保と解放を1ピクセルごとに繰り返すことにもなる
	//
	//	そこで実際の確保はこの単位へ切り上げ、要求が収まっている間は作り直さない
	//	表示は常にUVの部分矩形で切り出すため、見た目のサイズは1ピクセル単位で正確に追従する
	// -------------------------------------------------------------------------------
	constexpr uint32_t kSizeGranularity = 128;

	// 一度大きくした確保をどれだけ縮めたら作り直すか
	// これがないと、大→小へ動かしたあとメモリを持ち続けてしまう
	constexpr uint32_t kShrinkThreshold = kSizeGranularity * 2;

	// 要求サイズを粒度単位へ切り上げる
	uint32_t AlignUp(uint32_t _value, uint32_t _granularity)
	{
		return ((_value + _granularity - 1) / _granularity) * _granularity;
	}
}

ViewportTarget::ViewportTarget() = default;

ViewportTarget::~ViewportTarget()
{
	Term();
}

// -------------------------------------------------------------------------------
// 初期化
//
// この時点ではまだ表示先の大きさが決まっていないため、リソースは確保しない
// 実際の確保は、パネルの大きさが決まったあとのResizeで行う
// -------------------------------------------------------------------------------
bool ViewportTarget::Init(RHI::Device* _pDevice, EditorUIRenderer* _pRenderer)
{
	if (_pDevice == nullptr || _pRenderer == nullptr)
	{
		ELOG("ViewportTarget::Init() invalid argument");
		return false;
	}

	m_pDevice	= _pDevice;
	m_pRenderer	= _pRenderer;
	return true;
}

void ViewportTarget::Term()
{
	if (m_pDevice != nullptr)
	{
		// 破棄するリソースがGPUで使われていないことを保証する
		m_pDevice->WaitForGPU();
	}

	ReleaseResources();

	m_pDevice	= nullptr;
	m_pRenderer	= nullptr;
}

// -------------------------------------------------------------------------------
// 要求サイズに合わせてリソースを作り直す
// -------------------------------------------------------------------------------
bool ViewportTarget::Resize(uint32_t _width, uint32_t _height)
{
	if (m_pDevice == nullptr || m_pRenderer == nullptr)
	{ return false; }

	// 表示に必要な大きさ。UVの切り出しと縦横比にはこちらを使う
	const uint32_t viewWidth  = std::clamp(_width,  1u, kMaxSize);
	const uint32_t viewHeight = std::clamp(_height, 1u, kMaxSize);

	// 実際に確保する大きさは粒度単位へ切り上げる
	const uint32_t width  = std::clamp(AlignUp(viewWidth,  kSizeGranularity), kMinSize, kMaxSize);
	const uint32_t height = std::clamp(AlignUp(viewHeight, kSizeGranularity), kMinSize, kMaxSize);

	// -------------------------------------------------------------------------------
	// 作り直しが要るかを判定する
	//	1. 収まらない			… 大きくする必要がある
	//	2. 大幅に余っている		… メモリを抱え続けないよう縮める
	// -------------------------------------------------------------------------------
	if (IsValid())
	{
		const bool tooSmall =
			(width > m_Width) || (height > m_Height);

		const bool tooLarge =
			(m_Width  > width  + kShrinkThreshold) ||
			(m_Height > height + kShrinkThreshold);

		if (!tooSmall && !tooLarge)
		{
			// 今のリソースで足りている。表示範囲だけ更新して終わり
			m_ViewWidth  = viewWidth;
			m_ViewHeight = viewHeight;
			return true;
		}
	}

	// 描画中のリソースを解放しないよう、必ずGPUの完了を待ってから作り直す
	m_pDevice->WaitForGPU();
	ReleaseResources();

	auto* pD3DDevice = m_pDevice->GetDevice();

	// -------------------------------------------------------------------------------
	// 色バッファ（レンダーターゲット）
	// -------------------------------------------------------------------------------
	m_pColor = std::make_unique<RHI::ColorTarget>();
	if (!m_pColor->Init(
		pD3DDevice,
		m_pDevice->GetPool(RHI::Device::POOL_TYPE_RTV),
		width, height, kColorFormat, kClearColor))
	{
		ELOG("ViewportTarget::Resize() ColorTarget::Init failed");
		ReleaseResources();
		return false;
	}

	// -------------------------------------------------------------------------------
	// 深度バッファ
	// ゲーム画面はバックバッファとは別の深度が必要なため、専用に持つ
	// -------------------------------------------------------------------------------
	m_pDepth = std::make_unique<RHI::DepthTarget>();
	if (!m_pDepth->Init(
		pD3DDevice,
		m_pDevice->GetPool(RHI::Device::POOL_TYPE_DSV),
		width, height, kDepthFormat))
	{
		ELOG("ViewportTarget::Resize() DepthTarget::Init failed");
		ReleaseResources();
		return false;
	}

	// -------------------------------------------------------------------------------
	// 色バッファをテクスチャとして読むためのSRV
	// これをEditorUIに登録することで、ウィンドウの中身として貼れるようになる
	// -------------------------------------------------------------------------------
	m_pTexture = std::make_unique<RHI::Texture>();
	if (!m_pTexture->InitFromResource(
		pD3DDevice,
		m_pDevice->GetPool(RHI::Device::POOL_TYPE_RES),
		m_pColor->GetResource(),
		kColorFormat))
	{
		ELOG("ViewportTarget::Resize() Texture::InitFromResource failed");
		ReleaseResources();
		return false;
	}

	m_TextureId = m_pRenderer->RegisterTexture(m_pTexture.get());

	// デバッグレイヤーのメッセージでどのリソースか分かるように名前を付ける
	m_pColor->GetResource()->SetName(L"ViewportColor");
	m_pDepth->GetResource()->SetName(L"ViewportDepth");

	// -------------------------------------------------------------------------------
	// 生成直後の状態を追跡表へ登録する
	//
	//	色と深度の両方を必ず登録する
	//	解放されたリソースと同じアドレスに新しいリソースが載ることがあり、
	//	古い状態が残っていると「遷移元が実際と違う」バリアが発行されてしまう
	//	（その結果、コマンドリストのCloseが失敗して描画が完全に止まる）
	// -------------------------------------------------------------------------------
	auto* pTracker = m_pDevice->GetResourceStateTracker();
	pTracker->RegisterResource(m_pColor->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	pTracker->RegisterResource(m_pDepth->GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	// ReleaseResourcesで一度0に戻るため、確保後にまとめて確定させる
	m_Width			= width;
	m_Height		= height;
	m_ViewWidth		= viewWidth;
	m_ViewHeight	= viewHeight;

	return true;
}

// -------------------------------------------------------------------------------
// このターゲットへの描画を開始する
// -------------------------------------------------------------------------------
void ViewportTarget::Begin(ID3D12GraphicsCommandList* _pCmd)
{
	if (_pCmd == nullptr || !IsValid())
	{ return; }

	// 前フレームはテクスチャとして読まれているので、書き込める状態へ戻す
	auto* pTracker = m_pDevice->GetResourceStateTracker();
	pTracker->TransitionResource(m_pColor->GetResource(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	pTracker->FlushBarriers(_pCmd);

	auto handleRTV = m_pColor->GetHandleRTV()->HandleCPU;
	auto handleDSV = m_pDepth->GetHandleDSV()->HandleCPU;

	_pCmd->ClearRenderTargetView(handleRTV, kClearColor, 0, nullptr);
	_pCmd->ClearDepthStencilView(handleDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	_pCmd->OMSetRenderTargets(1, &handleRTV, FALSE, &handleDSV);

	// -------------------------------------------------------------------------------
	// ビューポートは「確保サイズ」ではなく「表示サイズ」に合わせる
	//
	// 確保はパネルより大きい粒度で行うため、確保サイズで描くと
	// パネルに映らない範囲まで描いてしまい、絵の縦横比もずれる
	// 実際に見える左上の領域だけを描画対象にする
	// -------------------------------------------------------------------------------
	const D3D12_VIEWPORT viewport
	{
		0.0f, 0.0f,
		static_cast<float>(m_ViewWidth), static_cast<float>(m_ViewHeight),
		0.0f, 1.0f
	};
	const D3D12_RECT scissor
	{
		0, 0,
		static_cast<LONG>(m_ViewWidth), static_cast<LONG>(m_ViewHeight)
	};

	_pCmd->RSSetViewports(1, &viewport);
	_pCmd->RSSetScissorRects(1, &scissor);
}

// -------------------------------------------------------------------------------
// 描画を終了し、テクスチャとして読める状態へ遷移させる
// -------------------------------------------------------------------------------
void ViewportTarget::End(ID3D12GraphicsCommandList* _pCmd)
{
	if (_pCmd == nullptr || !IsValid())
	{ return; }

	auto* pTracker = m_pDevice->GetResourceStateTracker();
	pTracker->TransitionResource(m_pColor->GetResource(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	pTracker->FlushBarriers(_pCmd);
}

// -------------------------------------------------------------------------------
// シーンの描画先としての指定を返す
// -------------------------------------------------------------------------------
SceneOutput ViewportTarget::GetSceneOutput() const
{
	SceneOutput output;

	if (!IsValid())
	{
		return output;	// 未確保。呼び出し側はバックバッファへ描くことになる
	}

	output.pColorResource	= m_pColor->GetResource();
	output.ColorRTV			= m_pColor->GetHandleRTV()->HandleCPU;
	output.pDepthResource	= m_pDepth->GetResource();
	output.DepthDSV			= m_pDepth->GetHandleDSV()->HandleCPU;
	// シーンには表示サイズを伝える
	// カメラの縦横比も中間バッファの大きさも、見える範囲を基準にする
	output.Width			= m_ViewWidth;
	output.Height			= m_ViewHeight;

	return output;
}

EditorUI::TextureId ViewportTarget::GetTextureId() const
{
	return m_TextureId;
}

float ViewportTarget::GetAspect() const
{
	if (m_ViewHeight == 0)
	{ return 1.0f; }

	return static_cast<float>(m_ViewWidth) / static_cast<float>(m_ViewHeight);
}

bool ViewportTarget::IsValid() const
{
	return m_pColor != nullptr && m_pDepth != nullptr && m_pTexture != nullptr;
}

// -------------------------------------------------------------------------------
// 現在のリソースを解放する
// -------------------------------------------------------------------------------
void ViewportTarget::ReleaseResources()
{
	if (m_pRenderer != nullptr && m_TextureId != 0)
	{
		m_pRenderer->UnregisterTexture(m_TextureId);
	}

	// 追跡表に古いポインタを残すと、同じアドレスに別リソースが載ったとき誤動作する
	if (m_pDevice != nullptr)
	{
		auto* pTracker = m_pDevice->GetResourceStateTracker();

		if (m_pColor != nullptr) { pTracker->UnRegisterResource(m_pColor->GetResource()); }
		if (m_pDepth != nullptr) { pTracker->UnRegisterResource(m_pDepth->GetResource()); }
	}

	m_pTexture.reset();
	m_pDepth.reset();
	m_pColor.reset();

	m_TextureId		= 0;
	m_Width			= 0;
	m_Height		= 0;
	m_ViewWidth		= 0;
	m_ViewHeight	= 0;
}
