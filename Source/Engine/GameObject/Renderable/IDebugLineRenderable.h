#pragma once

class DebugLineRenderQueue;

// -------------------------------------------------------------------------------
// IDebugLineRenderable
//
// 概要 :
//  DebugLineRenderQueueへデバッグ描画要求を登録できるComponent用インターフェース。
// -------------------------------------------------------------------------------
class IDebugLineRenderable
{
public:

	virtual ~IDebugLineRenderable() = default;

	virtual void SubmitDebugLine(
		DebugLineRenderQueue* _pQueue) const = 0;
};
