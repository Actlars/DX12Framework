// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "GraphicsView.h"
#include <Engine/RHI/Core/Device/Device.h>
#include <Engine/Renderer/Renderer.h>
#include <Engine/Utility/Debug/Logger/Logger.h>
#include <Engine/Test/CoopVecTestRunner/CoopVecTestRunner.h>
#include <Engine/Test/CoopVecMatMulTestRunner/CoopVecMatMulTestRunner.h>
#include <Engine/Test/NTCdecodeTestRunner/NTCdecodeTestRunner.h>
#include <Engine/Test/NTCImageTestRunner/NTCImageTestRunner.h>
#include <Engine/Renderer/Renderer.h>

// -------------------------------------------------------------------------------
// GraphicsView::Impl
// 
// 概要 : 
//	実際のRHI / Renderer一式を保持する。GraphicsView.hからは完全に不可視になる
// -------------------------------------------------------------------------------
class GraphicsView::Impl
{
public:

	RHI::Device		Device;
	Renderer		View;
};

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
GraphicsView::GraphicsView()
	: m_pImpl(std::make_unique<Impl>())
{ /* DO_NOTHING */ }

GraphicsView::~GraphicsView() = default;

// -------------------------------------------------------------------------------
//		初期化
// -------------------------------------------------------------------------------
bool GraphicsView::Init(void* _hWnd, uint32_t _width, uint32_t _height)
{
	RHI::Device::Desc desc;
	desc.hWnd			= static_cast<HWND>(_hWnd);
	desc.Width			= _width;
	desc.Height			= _height;
	desc.Format			= DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.DepthFormat	= DXGI_FORMAT_D32_FLOAT;
	desc.FrameCount		= 2;

	if (!m_pImpl->Device.Init(desc)) 
	{
		DLOG("GraphicsView::Init() Device failed");
		return false; 
	}

	if (!m_pImpl->View.Init(&m_pImpl->Device)) 
	{
		DLOG("GraphicsView::Init() Rendere failed");
		return false; 
	}

//#ifdef _DEBUG
//	{
//		CoopVecTestRunner coopVecTest;
//		coopVecTest.Run(&m_pImpl->Device);
//	}
//#endif
//
//#ifdef _DEBUG
//	{
//		NTCDecodeTestRunner ntcDecodeTest;
//		ntcDecodeTest.Run(&m_pImpl->Device);
//	}
//#endif

//#ifdef _DEBUG
//	{
//		NTCImageDecodeTestRunner ntcImageTest;
//		ntcImageTest.Run(&m_pImpl->Device);
//	}
//#endif

//#ifdef _DEBUG
//	{
//		CoopVecMatMulTestRunner coopVecMatMulTest;
//		coopVecMatMulTest.Run(&m_pImpl->Device);
//	}
//#endif

	return true;
}

// -------------------------------------------------------------------------------
//		終了
// -------------------------------------------------------------------------------
void GraphicsView::Term()
{
	m_pImpl->View.Term();
	m_pImpl->Device.Term();
}

// -------------------------------------------------------------------------------
//		フレーム開始処理
// -------------------------------------------------------------------------------
void* GraphicsView::BeginFrame()
{
	// ここでImplの中身に触れる。GraphicsView.cppの中だけで完結
	ID3D12GraphicsCommandList* pCmd = m_pImpl->View.BeginFrame();
	return pCmd;
}

// -------------------------------------------------------------------------------
//		フレームの終了
// -------------------------------------------------------------------------------
void GraphicsView::EndFrame(void* _pCmd)
{
	// void*として受け取ったものをここで本来の型に戻す
	auto* pCmd = static_cast<ID3D12GraphicsCommandList*>(_pCmd);
	m_pImpl->View.EndFrame(pCmd);
}

// -------------------------------------------------------------------------------
//		Present
// -------------------------------------------------------------------------------
void GraphicsView::Present(uint32_t _syncInterval)
{
	m_pImpl->View.Present(_syncInterval);
}

// -------------------------------------------------------------------------------
//		GPU完了待ち
// -------------------------------------------------------------------------------
void GraphicsView::WaitForGPU()
{
	m_pImpl->Device.WaitForGPU();
}

// -------------------------------------------------------------------------------
//		描画先の大きさをウィンドウに合わせる
//
//	Rendererはビューポートとシザーを初期化時のサイズで持っているため、
//	デバイス側を作り直したあとに必ず入れ直す
//	忘れると、大きくしたときに古い範囲までしか描かれなくなる
// -------------------------------------------------------------------------------
bool GraphicsView::Resize(uint32_t _width, uint32_t _height)
{
	if (!m_pImpl->Device.Resize(_width, _height))
	{
		return false;
	}

	// Renderer::Init はビューポートをデバイスの大きさから作る
	// 同じ処理を再利用して、新しい大きさへ更新する
	return m_pImpl->View.Init(&m_pImpl->Device);
}

// -------------------------------------------------------------------------------
//		フレームインデックスの更新
// -------------------------------------------------------------------------------
void GraphicsView::UpdateFrameIndexAfterSceneChange()
{
	m_pImpl->Device.UpdateFrameIndex();
}

// -------------------------------------------------------------------------------
//		現在のフレームインデックスを取得
// -------------------------------------------------------------------------------
uint32_t GraphicsView::GetFrameIndex() const
{
	return m_pImpl->Device.GetFrameIndex();
}

RHI::Device* GraphicsView::GetDevice()
{
	return &m_pImpl->Device;
}
