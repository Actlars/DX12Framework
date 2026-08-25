#pragma once

// -------------------------------------------------------------------------------
// SceneOutput struct
//
// 概要 :
//	「シーンの絵を最終的にどこへ描くか」を表す1つの指定
//
//	以前はバックバッファ固定だったが、ゲーム画面をエディタのパネルへ
//	表示するようになったため、描画先を差し替えられる必要が出てきた
//	描画先に関する情報をこの構造体1つへ集約することで、
//	SceneRendererとポストエフェクトが同じ情報を共有できる
//
//	所有権は持たない
//	実体はバックバッファ(RHI::Device)か、ViewportTargetのどちらかが持つ
// -------------------------------------------------------------------------------
struct SceneOutput
{
	// 最終出力先のカラーバッファ
	ID3D12Resource*				pColorResource	= nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE	ColorRTV{};

	// 深度バッファ。出力先と同じ大きさである必要がある
	ID3D12Resource*				pDepthResource	= nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE	DepthDSV{};

	// 描画範囲(ピクセル)。ビューポートと中間バッファの大きさに使う
	uint32_t Width	= 0;
	uint32_t Height	= 0;

	bool IsValid() const
	{
		return pColorResource != nullptr &&
			   pDepthResource != nullptr &&
			   Width > 0 && Height > 0;
	}

	// 縦横比。カメラの射影行列を出力先に合わせるために使う
	float GetAspect() const
	{
		return (Height > 0)
			? static_cast<float>(Width) / static_cast<float>(Height)
			: 1.0f;
	}
};
