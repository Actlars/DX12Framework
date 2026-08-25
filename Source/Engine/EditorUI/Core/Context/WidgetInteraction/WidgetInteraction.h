#pragma once
#include <Engine/EditorUI/Core/Id.h>
#include <Engine/EditorUI/Core/Types.h>
#include <Engine/EditorUI/Core/TextEditState.h>

// -------------------------------------------------------------------------------
// ウィジェットの入力所有権(ActiveId / HoveredId)
//
// 概要 :
//	即時モードでは毎フレームすべてのウィジェットが作り直されるため、
//	「今どのウィジェットがドラッグ／編集中か」をウィジェット側では覚えられない
//	そこでContextがIdを1つだけ握り、そのIdを持つウィジェットだけが
//	マウスやキーボードの入力を受け取れる、という排他制御を行う
//
//	これにより、重なったウィジェットを同時に操作してしまう問題や、
//	ドラッグ中にマウスが枠外へ出ると操作が途切れる問題を防いでいる
// -------------------------------------------------------------------------------
namespace EditorUI
{
	class WidgetInteraction
	{
	public:

		// 生存申告がなかったActiveIdと、持ち主のいなくなったテキスト編集状態を掃除する
		void NewFrame();

		Id		GetActiveId()		const;
		bool	IsActive(Id _id)	const;
		bool	IsAnyActive()		const;

		// 入力の所有権を握る。直前の所有者は自動的に解除される
		void SetActive(Id _id, const DirectX::XMFLOAT2& _mousePos);
		// 所有権を手放す。今の所有者が_id以外なら何もしない（横取り防止）
		void ClearActive(Id _id);
		// 今フレームも操作を続けている、という生存申告
		// これが呼ばれなかったActiveIdは、次のNewFrameで自動的に解除される
		void KeepAlive(Id _id);

		Id GetHoveredId() const;
		bool IsHoveredId(Id _id) const;
		void SetHoveredId(Id _id);

		// ActiveIdを獲得した瞬間のマウス位置。クリックとドラッグの判別に使う
		const DirectX::XMFLOAT2& GetActiveClickPos() const;

		// -------------------------------------------------------------------------------
		// Drag系ウィジェットが、1ピクセル未満の端数を持ち越すための作業領域
		//
		// 整数値のドラッグで「マウスを動かしても値が変わらない」現象を防ぐために使う
		// 操作中のウィジェットは常に1つだけなので、Contextが1つ持てば足りる
		// -------------------------------------------------------------------------------
		double& GetDragAccumulator();

		// キーボード編集中のテキスト状態（同時に1つだけ）
		TextEditState& GetTextEditState();
		const TextEditState& GetTextEditState() const;

	private:

		Id		m_ActiveId			= 0;		// 今フレーム、入力を占有しているウィジェット
		Id		m_HoveredId			= 0;		// 今フレーム、マウスが乗っているウィジェット
		bool	m_ActiveIdIsAlive	= false;	// ActiveIdの持ち主が今フレームも呼ばれたか

		DirectX::XMFLOAT2	m_ActiveIdClickPos{};	// ActiveId獲得時のマウス座標
		double				m_ActiveIdDragAccumulator = 0.0f;	// Drag系の端数の持ち越し

		TextEditState m_TextEdit;

	};
}
