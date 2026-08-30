#include "DockSpace.h"

// -------------------------------------------------------------------------------
// DockSpaceの初期化
// -------------------------------------------------------------------------------
void EditorUI::DockSpace::Init(const Rect2D& _rootBounds)
{
	// 以前作成されたDockNodeをすべて削除
	m_Nodes.clear();
	// DockNodeに割り当てるIDを先頭から振り直す
	m_NextNodeId = 0;

	// -------------------------------------------------------------------------------
	// ルートノードを作成
	// -------------------------------------------------------------------------------

	DockNode root;
	// ルートノードに一意なIDを割り当てる
	// 後置インクリメントなので、root.Idには現在の値が入り、
	// m_NextNodeIdは次に作成するノード用のIDに進む
	root.Id		= m_NextNodeId++;
	// 初期状態ではDockSpaceが分割されていないので、ルートノード自信をWindowを保持できるLeafノードとして扱う
	root.IsLeaf = true;
	// ルートノードはDockSpace全体の領域を使用するため、呼び出し側から渡された領域をそのまま設定する
	root.Bounds = _rootBounds;
	// DockSpaceの起点となるルートノードのIDを保持しておく。
	// 今後ノードツリーをたどる際は、このIDから探索を開始する
	m_RootId	= root.Id;
	// 作成したルートノードをノード管理コンテナへ登録する
	// 以降のDock分割やWindow配置はこのノードを基準に行われる
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

	_ratio = std::clamp(_ratio, 0.1f, 0.9f);

	DockNode& original = it->second;

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

	auto leafIt = m_Nodes.find(leafId);
	if (leafIt == m_Nodes.end() || !leafIt->second.IsLeaf)
	{ return; }

	DockNode& leaf = leafIt->second;

	auto windowIt = std::find(leaf.Windows.begin(), leaf.Windows.end(), _windowId);
	if (windowIt == leaf.Windows.end())
	{ return; }
	
	const int removeIndex = static_cast<int>(std::distance(leaf.Windows.begin(), windowIt));
	leaf.Windows.erase(windowIt);

	if (leaf.Windows.empty())
	{
		leaf.ActiveTabIndex = 0;
	}
	else
	{
		// アクティブタブより前を外した場合、同じタブを指定し続けるように詰める
		if (leaf.ActiveTabIndex > removeIndex)
		{
			--leaf.ActiveTabIndex;
		}

		leaf.ActiveTabIndex = std::clamp(leaf.ActiveTabIndex, 0, static_cast<int>(leaf.Windows.size()) - 1);
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
		if (!node.Bounds.Contains(_point))
		{
			return -1;
		}
		if (node.IsLeaf)
		{
			return node.Id;
		}

		auto childAIt = m_Nodes.find(node.ChildA);
		auto childBIt = m_Nodes.find(node.ChildB);
		if (childAIt == m_Nodes.end() || childBIt == m_Nodes.end())
		{
			return -1;
		}

		// ChildAに入っていた場合にelseで繋がないと、続くChildBの判定で
		// 「Bには入っていない」と見なされ、必ず-1が返ってしまう
		if (childAIt->second.Bounds.Contains(_point))
		{
			currentId = node.ChildA;
		}
		else if (childBIt->second.Bounds.Contains(_point))
		{
			currentId = node.ChildB;
		}
		else
		{
			return -1;
		}
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

// -------------------------------------------------------------------------------
// 全ノードを走査し、非LeafノードのSplit境界線付近に_pointがあるかを調べる
// -------------------------------------------------------------------------------
int EditorUI::DockSpace::FindSplitAt(const DirectX::XMFLOAT2& _point, float _hitThickness) const
{
	return FindSplitAtRecursive(m_RootId, _point, _hitThickness);
}

// -------------------------------------------------------------------------------
// Splitノードの比率を_pointの位置から再計算し、その場でサブツリーの矩形も更新
// -------------------------------------------------------------------------------
bool EditorUI::DockSpace::DragSplit(int _splitId, const DirectX::XMFLOAT2& _point, float _minRatio, float _maxRatio)
{
	auto it = m_Nodes.find(_splitId);
	if (it == m_Nodes.end() || it->second.IsLeaf) 
	{ return false; }

	DockNode& node = it->second;
	const Rect2D bounds = node.Bounds;	// 再計算前にコピーしておく

	if (bounds.Width() <= 0.0f || bounds.Height() <= 0.0f)
	{ return false; }
	
	float newRatio = node.SplitHorizontal
		? (_point.x - bounds.Min.x) / bounds.Width()
		: (_point.y - bounds.Min.y) / bounds.Height();

	newRatio = std::clamp(newRatio, _minRatio, _maxRatio);

	// 求めた比率をノードへ書き戻す
	// これを忘れるとRecomputeBoundsが古い比率で再計算するため、境界線が動かない
	node.SplitRatio = newRatio;

	// このノード配下だけ即座に再計算し、ドラッグ中も同一フレームで見た目に対応させる
	RecomputeBounds(_splitId, bounds);
	return true;
}

// -------------------------------------------------------------------------------
// ルート領域そのものを分割し、画面の外周に新しいLeafを作る
//
// ウィンドウを画面の外までドラッグしたときの受け皿
// 通常のSplitLeafは「マウス直下のLeaf」を割るのに対し、こちらは常に画面全体を割るため
// UE5のように画面の端へ吸着させる挙動になる
// -------------------------------------------------------------------------------
int EditorUI::DockSpace::SplitRoot(DockSplitDir _dir, float _ratio)
{
	if (m_RootId == -1)
	{ return -1; }

	auto rootIt = m_Nodes.find(m_RootId);
	if (rootIt == m_Nodes.end())
	{ return -1; }

	// ルートがまだ空のLeafなら、割らずにそのまま使う
	// 何もドッキングしていない状態で外周へ落としたとき、無駄な分割を作らないため
	if (rootIt->second.IsLeaf && rootIt->second.Windows.empty())
	{
		return m_RootId;
	}

	// ルートがLeafならそのまま分割できる
	// SplitLeafは領域を配り直さないため、ここで明示的に更新する
	// 同じフレーム内で続けて分割・検索を行う（既定レイアウトの構築）ために必要
	if (rootIt->second.IsLeaf)
	{
		const Rect2D bounds = rootIt->second.Bounds;
		const int newLeafId = SplitLeaf(m_RootId, _dir, _ratio);

		if (newLeafId != -1)
		{
			RecomputeBounds(m_RootId, bounds);
		}

		return newLeafId;
	}

	// ルートが既にSplitの場合は、ルートを1段深くして外周のLeafを新設する
	// ルート自身のIdを変えないために、既存のルートの中身を子ノードへ退避させる
	const Rect2D rootBounds = rootIt->second.Bounds;

	DockNode existing		= rootIt->second;	// 既存のレイアウト一式
	existing.Id				= m_NextNodeId++;
	existing.Parent			= m_RootId;

	// 退避した側が持つ子の親参照を、新しいIdへ張り替える
	if (!existing.IsLeaf)
	{
		auto childAIt = m_Nodes.find(existing.ChildA);
		auto childBIt = m_Nodes.find(existing.ChildB);
		if (childAIt == m_Nodes.end() || childBIt == m_Nodes.end())
		{ return -1; }

		childAIt->second.Parent = existing.Id;
		childBIt->second.Parent = existing.Id;
	}

	DockNode newLeaf;
	newLeaf.Id		= m_NextNodeId++;
	newLeaf.Parent	= m_RootId;
	newLeaf.IsLeaf	= true;

	const float ratio		= std::clamp(_ratio, 0.1f, 0.9f);
	const bool  horizontal	= (_dir == DockSplitDir::Left || _dir == DockSplitDir::Right);
	const bool  newSideIsA	= (_dir == DockSplitDir::Left || _dir == DockSplitDir::Top);

	DockNode& root			= rootIt->second;
	root.IsLeaf				= false;
	root.SplitHorizontal	= horizontal;
	root.SplitRatio			= newSideIsA ? ratio : (1.0f - ratio);
	root.Windows.clear();
	root.ActiveTabIndex		= 0;
	root.ChildA				= newSideIsA ? newLeaf.Id : existing.Id;
	root.ChildB				= newSideIsA ? existing.Id : newLeaf.Id;

	m_Nodes.emplace(existing.Id, existing);
	m_Nodes.emplace(newLeaf.Id, newLeaf);

	// 追加した分を含めて領域を配り直す
	RecomputeBounds(m_RootId, rootBounds);

	return newLeaf.Id;
}

EditorUI::Rect2D EditorUI::DockSpace::GetRootBounds() const
{
	auto it = m_Nodes.find(m_RootId);
	return (it != m_Nodes.end()) ? it->second.Bounds : Rect2D{};
}

const EditorUI::DockNode* EditorUI::DockSpace::GetNode(int _nodeId) const
{
	auto it = m_Nodes.find(_nodeId);
	return (it != m_Nodes.end()) ? &it->second : nullptr;
}

bool EditorUI::DockSpace::IsSingleEmptyRoot() const
{
	if (m_Nodes.size() != 1)
	{
		return false;
	}

	auto it = m_Nodes.find(m_RootId);
	return it != m_Nodes.end() && it->second.IsLeaf && it->second.Windows.empty();
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

	// Splitノードは空leafではない
	if (!it->second.IsLeaf) 
	{ return; }

	const int parentId = it->second.Parent;
	if (parentId == -1)
	{ return; }	//ルート自体は消さない

	auto parentIt = m_Nodes.find(parentId);
	if (parentIt == m_Nodes.end() || parentIt->second.IsLeaf) 
	{ return; }

	const DockNode parentCopy = parentIt->second;
	// 空になった方ではない、もうた片方の兄弟ノードの中身を親ノードの場所にそのまま昇格させる
	// これにより不要になったSplitノード1つと、空になったLeafノード１つが木から消える
	int siblingId = -1;
	if (parentCopy.ChildA == _leafId)
	{
		siblingId = parentCopy.ChildB;
	}
	else if (parentCopy.ChildB == _leafId)
	{
		siblingId = parentCopy.ChildA;
	}
	else
	{
		return;
	}

	auto siblingIt = m_Nodes.find(siblingId);
	if (siblingIt == m_Nodes.end())
	{
		return;
	}

	DockNode promoted	= siblingIt->second;
	promoted.Id			= parentId;
	promoted.Parent		= parentCopy.Parent;

	// siblingがSplitだった場合、その子のParentも親の新しいIdに向け直す必要がある
	if (!promoted.IsLeaf)
	{
		auto childAIt = m_Nodes.find(promoted.ChildA);
		auto childBIt = m_Nodes.find(promoted.ChildB);
		if (childAIt == m_Nodes.end() || childBIt == m_Nodes.end())
		{
			return;
		}

		childAIt->second.Parent = parentId;
		childBIt->second.Parent = parentId;
	}

	m_Nodes.erase(_leafId);	// 空になったLeafを削除
	m_Nodes.erase(siblingId);
	m_Nodes[parentId] = promoted;

	// 昇格されたノードが空のLeafである可能性もあるので、再帰的にもう一度確認する
	MergeUpIfEmpty(parentId);
}

int EditorUI::DockSpace::FindSplitAtRecursive(int _nodeId, const DirectX::XMFLOAT2& _point, float _hitThickness) const
{
	auto it = m_Nodes.find(_nodeId);
	if (it == m_Nodes.end() || it->second.IsLeaf)
	{
		return -1;
	}

	const DockNode& node = it->second;

	// 子を先に調べ、より深いSplitを優先する
	if (const int childAResult =
		FindSplitAtRecursive(node.ChildA, _point, _hitThickness);
		childAResult != -1)
	{
		return childAResult;
	}

	if (const int childBResult =
		FindSplitAtRecursive(node.ChildB, _point, _hitThickness);
		childBResult != -1)
	{
		return childBResult;
	}

	const Rect2D& bounds = node.Bounds;

	if (node.SplitHorizontal)
	{
		const float splitX = bounds.Min.x + bounds.Width() * node.SplitRatio;
		const bool inRange = (_point.y >= bounds.Min.y && _point.y <= bounds.Max.y);
		return inRange && std::abs(_point.x - splitX) <= _hitThickness ? node.Id : -1;
	}

	const float splitY = bounds.Min.y + bounds.Height() * node.SplitRatio;
	const bool inRange = (_point.x >= bounds.Min.x && _point.x <= bounds.Max.x);
	return inRange && std::abs(_point.y - splitY) <= _hitThickness ? node.Id : -1;
}
