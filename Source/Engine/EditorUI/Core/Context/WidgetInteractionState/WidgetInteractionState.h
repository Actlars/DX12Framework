#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Id.h>
#include <Engine/EditorUI/Core/TextEditState.h>
#include <Engine/EditorUI/Core/Types.h>

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// WidgetInteractionState class
	// 
	// 概要 : 
	//	Widgetの一時的な操作状態を所有する
	// -------------------------------------------------------------------------------
	class WidgetInteractionState
	{
	public:

		void NewFrame();

		Id   GetActiveId() const;
		bool IsActive(Id _id) const;
		bool IsAnyActive() const;

		void SetActive(Id _id, const DirectX::XMFLOAT2& _mousePos);
		void ClearActive(Id _id);
		void KeepActive(Id _id);

		Id	 GetHoveredId() const;
		bool IsHovered(Id _id) const;
		void SetHovered(Id _id);

		const DirectX::XMFLOAT2& GetActiveClickPos() const;
		double& GetDragAccumulator();

		TextEditState& GetTextEditState();
		const TextEditState& GetTextEditState() const;

	private:

		Id m_ActiveId = 0;
		Id m_HoveredId = 0;
		bool m_ActiveIdIsAlive = false;

		DirectX::XMFLOAT2 m_ActiveIdClickPos{};
		double m_ActiveIdDragAccumulator = 0.0f;

		TextEditState m_TextEdit;

	};
}