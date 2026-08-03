#include "DockSpace.h"

void EditorUI::DockSpace::Init(const Rect2D& _rootBounds)
{
	m_Nodes.clear();
	m_NextNodeId = 0;

	DockNode root;
	root.Id		= m_NextNodeId++;
	root.IsLeaf = true;
	root.Bounds = _rootBounds;
	m_RootId	= root.Id;
	m_Nodes.emplace(root.Id, root);
}

void EditorUI::DockSpace::UpdateLayout(const Rect2D& _rootBounds)
{
	if (m_RootId != -1)
	{
		RecomputeBounds(m_RootId, _rootBounds);
	}
}

int EditorUI::DockSpace::SplitLeaf(int _leafId, DockSplitDir _dir, float _ratio)
{
	auto it = m_Nodes.find(_leafId);
	if (it == m_Nodes.end() || !it->second.IsLeaf)
	{ return -1; }	// 対象が存在しない、またはすでにSplit済みのノードは分割できない
	if (_dir == DockSplitDir::None || _dir == DockSplitDir::Center)
	{
		return -1;	// Centerはタブ化で扱うべきで、ここには来ない想定
	}

	DockNode& original = it->second;
	const int parentId = original.Parent;

	// 元のLeafの中身(ウィンドウ一覧)を新しいChildA用ノードへそのまま移す
	// 元のノードId自体はSplit役に転生させ、木構造上の位置(親からの参照)を
	// 一切変更せずにすむようにする
	DockNode childANode;
	childANode.Id				= m_NextNodeId++;
	childANode.Parent			= _leafId;
	childANode.IsLeaf			= true;
	childANode.Windows			= std::move(original.Windows);
	childANode.ActiveTabIndex	= original.ActiveTabIndex;

	DockNode childBNode;
	childBNode.Id		= m_NextNodeId++;
	childBNode.Parent	= _leafId;
	childBNode.IsLeaf	= true;
	// Windowsは空のまま。呼び出し側がDockWindowIntoLeafで詰める
	const bool horizontal = (_dir == DockSplitDir::Left || _dir == DockSplitDir::Right);
	// 新規ウィンドウ側(ドロップされた側)が「Left/Top」ならChildA、「Right/Bottom」ならChildBに来るように並べる
	const bool _newSideIsA = (_dir == DockSplitDir::Left || _dir == DockSplitDir::Top);

	original.IsLeaf				= false;
	original.SplitHorizontal	= horizontal;
	original.SplitRatio			= _newSideIsA ? _ratio : (1.0f - _ratio);
	original.Windows.clear();
	original.ActiveTabIndex		= 0;

	int newLeafId = 0;
	if (_newSideIsA)
	{
		original.ChildA = childBNode.Id;	// 新規Leafを先に確保するため、ここではchildBNodeを新規側として扱う
		original.ChildB = childANode.Id;
		newLeafId		= childBNode.Id;
	}
	else
	{
		original.ChildA = childANode.Id;
		original.ChildB = childBNode.Id;
		newLeafId		= childBNode.Id;
	}

	m_Nodes.emplace(childANode.Id, childANode);
	m_Nodes.emplace(childBNode.Id, childBNode);

	return newLeafId;
}

void EditorUI::DockSpace::DockWindowIntoLeaf(int _leafId, Id _windowId)
{
	auto targetIt = m_Nodes.find(_leafId);
	if (targetIt == m_Nodes.end() || !targetIt->second.IsLeaf)
	{ return; }

	const int sourceLeafId = FindLeafOwning(_windowId);

	// 既に同じLeafに所属している場合は、追加せずアクティブ化だけ行う
	if (sourceLeafId == _leafId)
	{
		DockNode& targetLeaf = targetIt->second;
		auto windowIt = std::find(targetLeaf.Windows.begin(), targetLeaf.Windows.end(), _windowId);
		if (windowIt != targetLeaf.Windows.end())
		{
			targetLeaf.ActiveTabIndex = static_cast<int>(std::distance(targetLeaf.Windows.begin(), windowIt));
		}
		return;
	}

	// 移動元から取り除く
	if (sourceLeafId != -1)
	{
		auto sourceIt = m_Nodes.find(sourceLeafId);
		if (sourceIt != m_Nodes.end())
		{
			DockNode& sourceLeaf = sourceIt->second;

			auto windowIt = std::find(sourceLeaf.Windows.begin(), sourceLeaf.Windows.end(), _windowId);

			if (windowIt != sourceLeaf.Windows.end())
			{
				const int removeIndex = static_cast<int>(std::distance(sourceLeaf.Windows.begin(), windowIt));

				sourceLeaf.Windows.erase(windowIt);

				if (sourceLeaf.Windows.empty())
				{
					sourceLeaf.ActiveTabIndex = 0;
				}
				else
				{
					// アクティブタブより前のタブが消えた場合はインデックスをずらす
					if (sourceLeaf.ActiveTabIndex > removeIndex)
					{
						--sourceLeaf.ActiveTabIndex;
					}

					sourceLeaf.ActiveTabIndex = std::clamp(sourceLeaf.ActiveTabIndex, 0, static_cast<int>(sourceLeaf.Windows.size()) - 1);
				}
			}
		}
	}

	// まだツリーを縮約していないため、移動先はLeafは存在する
	targetIt = m_Nodes.find(_leafId);
	if (targetIt == m_Nodes.end() || !targetIt->second.IsLeaf)
	{ return; }

	DockNode& targetLeaf = targetIt->second;
	targetLeaf.Windows.push_back(_windowId);
	targetLeaf.ActiveTabIndex = static_cast<int>(targetLeaf.Windows.size()) - 1;

	// 移動先に追加してから、空になった移動元を縮約する
	if (sourceLeafId != -1)
	{
		MergeUpIfEmpty(sourceLeafId);
	}

	//// 既に別のLeafにドッキング済みなら、まずそちらから外す
	//UndockWindow(_windowId);

	//DockNode& leaf = it->second;
	//leaf.Windows.push_back(_windowId);
	//leaf.ActiveTabIndex = static_cast<int>(leaf.Windows.size()) - 1;	// 新規追加したタブをアクティブにする
}

void EditorUI::DockSpace::UndockWindow(Id _windowId)
{
	const int leafId = FindLeafOwning(_windowId);
	if (leafId == -1) 
	{ return; }

	DockNode& leaf = m_Nodes[leafId];
	auto windowIt = std::find(leaf.Windows.begin(), leaf.Windows.end(), _windowId);
	if (windowIt != leaf.Windows.end())
	{
		leaf.Windows.erase(windowIt);
		leaf.ActiveTabIndex = std::clamp(leaf.ActiveTabIndex, 0, (std::max)(0, static_cast<int>(leaf.Windows.size()) - 1));
	}

	MergeUpIfEmpty(leafId);	// Leafが空になった場合は木から消す
}

int EditorUI::DockSpace::FindLeafAt(const DirectX::XMFLOAT2& _point) const
{
	int currentId = m_RootId;
	while (currentId != -1)
	{
		auto it = m_Nodes.find(currentId);
		if (it == m_Nodes.end())
		{ return -1; }
		const DockNode& node = it->second;
		if (node.IsLeaf)
		{
			return node.Bounds.Contains(_point) ? node.Id : -1;
		}

		// 子のどちらかの領域にpointが入っているかで降りていく
		const DockNode& childA = m_Nodes.at(node.ChildA);
		currentId = childA.Bounds.Contains(_point) ? node.ChildA : node.ChildB;
	}
	return -1;
}

int EditorUI::DockSpace::FindLeafOwning(Id _windowId) const
{
	for (const auto& [id, node] : m_Nodes)
	{
		if(node.IsLeaf && std::find(node.Windows.begin(), node.Windows.end(), _windowId)!= node.Windows.end())
		{
			return id;
		}
	}
	return -1;
}

bool EditorUI::DockSpace::IsLeafOccupied(int _leafId) const
{
	auto it = m_Nodes.find(_leafId);
	return (it != m_Nodes.end() && it->second.IsLeaf && !it->second.Windows.empty());
}

void EditorUI::DockSpace::SetActiveTab(int _leafId, int _tabIndex)
{
	auto it = m_Nodes.find(_leafId);
	if (it == m_Nodes.end() || !it->second.IsLeaf) 
	{ return; }
	if (_tabIndex >= 0 && _tabIndex < static_cast<int>(it->second.Windows.size()))
	{
		it->second.ActiveTabIndex = _tabIndex;
	}
}

const EditorUI::DockNode* EditorUI::DockSpace::GetNode(int _nodeId) const
{
	auto it = m_Nodes.find(_nodeId);
	return (it != m_Nodes.end()) ? &it->second : nullptr;
}

void EditorUI::DockSpace::RecomputeBounds(int _nodeId, const Rect2D& _bounds)
{
	auto it = m_Nodes.find(_nodeId);
	if (it == m_Nodes.end())
	{ return; }

	DockNode& node = it->second;
	node.Bounds = _bounds;

	if (node.IsLeaf) 
	{ return; }	// Leafには子がないのでここで終わり

	// SplitRatioにしたがって、自分の領域を2つの子の領域に分割する
	if (node.SplitHorizontal)
	{
		const float splitX = _bounds.Min.x + _bounds.Width() * node.SplitRatio;
		RecomputeBounds(node.ChildA, { _bounds.Min, {splitX, _bounds.Max.y} });
		RecomputeBounds(node.ChildB, { {splitX, _bounds.Min.y}, _bounds.Max });
	}
	else
	{
		const float splitY = _bounds.Min.y + _bounds.Height() * node.SplitRatio;
		RecomputeBounds(node.ChildA, { _bounds.Min, {_bounds.Max.x, splitY} });
		RecomputeBounds(node.ChildB, { {_bounds.Min.x, splitY}, _bounds.Max });
	}
}

void EditorUI::DockSpace::MergeUpIfEmpty(int _leafId)
{
	auto it = m_Nodes.find(_leafId);
	if (it == m_Nodes.end() || !it->second.Windows.empty())
	{ return; }	// まだウィンドウが残っているなら、Leafのまま残す

	const int parentId = it->second.Parent;
	if (parentId == -1)
	{ return; }	//ルート自体は消さない

	DockNode& parent = m_Nodes[parentId];
	// 空になった方ではない、もうた片方の兄弟ノードの中身を親ノードの場所にそのまま昇格させる
	// これにより不要になったSplitノード1つと、空になったLeafノード１つが木から消える
	const int siblingId = (parent.ChildA == _leafId) ? parent.ChildB : parent.ChildA;
	DockNode sibling = m_Nodes[siblingId];	// コピーをとってから、後で親の位置に上書きする

	const int grandParentId = parent.Parent;
	sibling.Id = parentId;	// 親の場所を乗っ取る形にする
	sibling.Parent = grandParentId;

	// siblingがSplitだった場合、その子のParentも親の新しいIdに向け直す必要がある
	if (!sibling.IsLeaf)
	{
		m_Nodes[sibling.ChildA].Parent = parentId;
		m_Nodes[sibling.ChildB].Parent = parentId;
	}

	m_Nodes.erase(_leafId);	// 空になったLeafを削除
	m_Nodes.erase(siblingId);
	m_Nodes[parentId] = sibling;

	// ルートが消えた場合の付け替え
	if (m_RootId == _leafId || m_RootId == siblingId)
	{
		m_RootId = parentId;
	}

	// 昇格されたノードが空のLeafである可能性もあるので、再帰的にもう一度確認する
	MergeUpIfEmpty(parentId);
}
