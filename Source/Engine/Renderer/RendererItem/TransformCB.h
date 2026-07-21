#pragma once

// 定数バッファ（ワールド・ビュー・プロジェクション）
// MeshComponent自身が持ち、TrnsformComponentから毎フレーム更新する
struct alignas(256) TransformCB
{
	DirectX::XMMATRIX World;
	DirectX::XMMATRIX View;
	DirectX::XMMATRIX Proj;
};
