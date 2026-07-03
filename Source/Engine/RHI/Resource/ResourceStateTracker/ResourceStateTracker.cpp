// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "ResourceStateTracker.h"

// -------------------------------------------------------------------------------
//		コンストラクタ
// -------------------------------------------------------------------------------
RHI::ResourceStateTracker::ResourceStateTracker()
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		デストラクタ
// -------------------------------------------------------------------------------
RHI::ResourceStateTracker::~ResourceStateTracker() 
{ /* DO_NOTHING */ }

// -------------------------------------------------------------------------------
//		リソースの初期ステートを登録
// -------------------------------------------------------------------------------
void RHI::ResourceStateTracker::RegisterResource(ID3D12Resource* _pResource, D3D12_RESOURCE_STATES _initialState)
{
	if (_pResource == nullptr) 
	{ return; }

	std::lock_guard<std::mutex> lock(m_Mutex);
	m_ResourceStates[_pResource] = _initialState;
}

// -------------------------------------------------------------------------------
//		リソースを新しいステートに遷移させる要求を行う
// -------------------------------------------------------------------------------
void RHI::ResourceStateTracker::TransitionResource(ID3D12Resource* _pResource, D3D12_RESOURCE_STATES _newState, UINT _subResource)
{
	if (_pResource == nullptr) 
	{ return; }

	std::lock_guard<std::mutex> lock(m_Mutex);
	auto it = m_ResourceStates.find(_pResource);
	if (it == m_ResourceStates.end())
	{
		// 未登録リソース : 初回アクセスとして現在ステート = 要求ステートで記録
		// 本来はRegisterResourceで登録しておくべき
		m_ResourceStates[_pResource] = _newState;
	}

	// すでに同じステートならバリア不要
	if (it->second == _newState) 
	{ return; }

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type					= D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags					= D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource	= _pResource;
	barrier.Transition.StateBefore	= it->second;
	barrier.Transition.StateAfter	= _newState;
	barrier.Transition.Subresource	= _subResource;

	m_PendingBarriers.emplace_back(barrier);
	it->second = _newState;	// 発行前だが、ペンディング分も含めて「これから遷移する」ことを記録
}

// -------------------------------------------------------------------------------
//		溜めておいたリソースバリアをコマンドリストに一括で発行する
// -------------------------------------------------------------------------------
void RHI::ResourceStateTracker::FlushBarriers(ID3D12GraphicsCommandList* _pCmd)
{
	if (_pCmd == nullptr) 
	{ return; }

	std::lock_guard<std::mutex> lock(m_Mutex);

	if (!m_PendingBarriers.empty())
	{
		_pCmd->ResourceBarrier(static_cast<UINT>(m_PendingBarriers.size()), m_PendingBarriers.data());

		m_PendingBarriers.clear();
	}
}

// -------------------------------------------------------------------------------
//		リソース破棄。追跡対象から外す
// -------------------------------------------------------------------------------
void RHI::ResourceStateTracker::UnRegisterResource(ID3D12Resource* _pResource)
{
	if (_pResource == nullptr) 
	{ return; }

	std::lock_guard<std::mutex> lock(m_Mutex);
	m_ResourceStates.erase(_pResource);
}
