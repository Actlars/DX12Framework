# Undo / Redo・コンポーネント操作・プレファブ 変更まとめ（第5回）

対象 : Editor / Application / Engine::GameObject / Engine::EditorUI
ビルド : Debug x64 でエラー・警告 0。
起動して一連の操作を行っても、D3D12 デバッグレイヤーのメッセージは 0。終了も正常。

今回の依頼は次の5点。

1. Undo / Redo
2. ウィンドウサイズの保存
3. コンポーネントの付与と削除
4. 各オブジェクトの名前の変更
5. プレファブ機能は必要か。必要なら作成

---

## 0. 全体の考え方

「シーンを書き換える操作」を、**必ず1つの入口に通す**形へ整理しました。

```
  メニューバー ─┐
  ヒエラルキー ─┼→ ObjectCommands ─→ CommandHistory ─→ GameObjectFactory ─→ GameObjectManager
  インスペクタ ─┘   ComponentCommands   （履歴）        （生成・出し入れ）   （所有と寿命）
```

以前は各パネルが `GameObjectFactory` を直接呼んでいました。
このままUndoを足すと「メニューからの削除は戻せるが、ヒエラルキーからの削除は戻せない」
といった、使う人には理由の分からない差が生まれます。

そこで層を1つ増やし、

| 層 | 担当 |
|---|---|
| 各パネル / メニュー | **いつ**呼ぶか（どこにボタンを置くか） |
| ObjectCommands / ComponentCommands | 1回分の操作としてまとめ、履歴へ登録する |
| CommandHistory | 操作を並べて覚え、行き来させる |
| GameObjectFactory | 生成と、シーンへの出し入れ（履歴を知らない） |
| GameObjectManager | オブジェクトの所有と寿命（エンジン側） |

としました。パネル側から「履歴を通さずに書き換える」経路が無くなるため、
**取り消せない操作が混ざることが構造的に起きません。**

---

## 1. Undo / Redo

### 1-1. 操作を「もの」として扱う

取り消せる操作1つを `IEditorCommand` で表します。

```cpp
class IEditorCommand
{
public:
    virtual bool Execute(EditorContext&) = 0;   // 最初の実行 / やり直し
    virtual void Undo(EditorContext&)    = 0;   // 取り消し
    virtual std::string_view GetLabel() const = 0;  // メニューに出す名前
};
```

書き換え方と戻し方が必ず同じクラスに並ぶため、
**「戻す処理だけ実装し忘れる」ということが起きにくい**のがこの形の利点です。

`CommandHistory` は山（スタック）を2つ持つだけの素朴なつくりです。

| 操作 | 動き |
|---|---|
| 実行 | 操作を行い「戻せる山」へ積む。「やり直せる山」は捨てる |
| 元に戻す | 戻せる山から1つ取り出して `Undo`。やり直せる山へ移す |
| やり直す | やり直せる山から1つ取り出して `Execute`。戻せる山へ戻す |

新しい操作を行った時点でやり直せる山を捨てるのは、
そこから先の未来が分岐して、もう再現できなくなるためです。

### 1-2. いちばん重要な設計 — 削除しても「壊さない」

Undoで一番難しいのは**削除**です。
破棄してしまうと、取り消すときに作り直すしかありません。
作り直したオブジェクトは

* IDが変わる
* ポインタが変わる（他のコマンドが指していた先が無効になる）
* コンポーネントが持っていた初期化状態が失われる

ため、「元に戻した」とは言えないものになります。

そこで `GameObjectManager` に **Detach** を追加しました。

```cpp
// シーンから外すが、実体は壊さず所有権を呼び出し側へ渡す
std::unique_ptr<GameObject> Detach(GameObject* _pObject);
```

| | Remove（従来） | Detach（今回追加） |
|---|---|---|
| シーンから消える | ○ | ○ |
| 実体 | 破棄される | **履歴が預かる** |
| 元に戻せるか | × | ○（まったく同じものが戻る） |

削除されたオブジェクトはコマンドの中で生き続け、取り消すとそのまま戻ります。
検証では**削除→取り消しでIDが 8 のまま変わらない**ことを確認しました。

なお作成と削除は同じ操作の向き違いでしかないため、
`ObjectLifetimeCommand` 1つに向きだけを持たせています。

```
作成 : 実行 = 置く    / 取り消し = 取り除く
削除 : 実行 = 取り除く / 取り消し = 置く
```

2つのクラスに分けると、片方だけ直したときに挙動がずれるためです。

### 1-3. ドラッグ1回 = Undo 1回

Transformの編集はドラッグ中、毎フレーム値が変わります。
変化のたびに履歴へ積むと、1回動かしただけで何十回もUndoが必要になります。

そこで

* 値が動き始めた最初のフレームで「編集前の値」を控える
* **どのウィジェットも掴まれていない状態になったら**、1回だけ履歴へ積む

という形にしました。見た目はドラッグに追従したまま、履歴には1件だけ残ります。

### 1-4. 寿命の約束

履歴はGameObjectを生ポインタで持ちます。安全のため次の規則を守っています。

* 触る前に必ず `GameObjectFactory::IsAlive()` で生存を確認する
* 履歴を捨てるときは**必ず時系列の端から**まとめて捨てる
  （やり直せる山は「ある時点から後ろ全部」、上限超過は「いちばん古い方から」）
* **シーンが切り替わったら履歴をすべて捨てる**
  前のシーンのオブジェクトを、別のシーンへ復元してしまわないため

覚えておく操作は100件までとしました。
削除されたオブジェクトの実体も抱えるため、無制限だと使うほどメモリが増え続けます。

### 1-5. 操作方法

| 操作 | 場所 |
|---|---|
| 元に戻す | `Ctrl+Z` / 編集メニュー |
| やり直す | `Ctrl+Y` / `Ctrl+Shift+Z` / 編集メニュー |
| 複製 | `Ctrl+D` |
| 削除 | `Delete` |
| 名前を変更 | `F2` |

メニューには「元に戻す **: オブジェクトの削除**」のように直前の操作名を添えています。
何が戻るのかを押す前に分かるようにするためです。

文字を入力している最中はショートカットを無視します。
名前の編集中に `Ctrl+Z` を押して、入力欄ではなくシーンが巻き戻るのを防ぐためです。

---

## 2. ウィンドウサイズの保存

毎回同じ大きさで開き直されると、そのたびに並べ直すことになります。
終了時の状態を書き出し、次の起動でそこから始められるようにしました。

保存先 : `Config/Window.json`（リポジトリには含めません。`.gitignore` に追加済み）

```json
{
    "PositionX": 120,
    "PositionY": 90,
    "Width": 1024,
    "Height": 720,
    "Maximized": false
}
```

### 気を付けた点

**保存するタイミング** — 終了処理で保存しようとすると失敗します。
`WM_CLOSE` のあと `DefWindowProc` がウィンドウを破棄するため、
そのあとでは `GetWindowPlacement` が値を返せません。
`WM_CLOSE` を受けた時点で保存しています。
（この点は最初の実装で実際に保存されず、検証で判明したため修正しました）

**`GetWindowRect` ではなく `GetWindowPlacement`** — 前者は「今見えている矩形」を返すため、
最大化中に保存すると画面いっぱいの大きさが記録されます。
次の起動で最大化を解除しても画面いっぱいのままになってしまいます。
`GetWindowPlacement` なら「元に戻したときの矩形」と最大化状態を別々に取得できます。

**画面の外に出ていないか** — モニタを外したあとに前回の位置をそのまま使うと、
見えない場所にウィンドウが開いて操作できなくなります。
`MonitorFromRect` でどのモニタにも重ならない場合は、既定の位置で開きます。

**責務の分け方** — `WindowSettings` は値の保持とファイルの読み書きだけを持ち、
Win32のAPIは一切知りません。ウィンドウから値を取り出すのは `Application` の担当です。

---

## 3. コンポーネントの付与と削除

### 3-1. なぜ登録表が必要か

`GameObject::AddComponent<T>()` はテンプレートです。
そのままでは「メニューで選ばれた名前のコンポーネントを付ける」ことが書けません。

そこで型ごとの手順を関数ポインタとして表に持つ `ComponentRegistry` を作りました。
画面側は表を引くだけで、どの型でも同じように扱えます。

```cpp
struct ComponentTypeInfo
{
    std::string_view TypeName;      // "TransformComponent"
    std::string_view DisplayName;   // "Transform"
    bool IsRemovable;               // Transformのように外せないものはfalse

    bool       (*Has)   (const GameObject&);
    Component* (*Add)   (GameObject&);
    bool       (*Remove)(GameObject&);

    void (*Save)(const GameObject&, nlohmann::json&);   // 値の写し取り
    void (*Load)(GameObject&, const nlohmann::json&);   // 値の復元

    void (*DrawInspector)(EditorContext&, GameObject&); // 画面での見せ方
};
```

**新しいコンポーネントをエディタ対応させるときは、表へ1項目足すだけです。**
インスペクタの表示・追加メニュー・複製・プレファブがまとめて対応します。

現在の登録は Transform / Mesh / Meshlet の3種類です。

### 3-2. Save / Load を1本にした理由

次の3つは、どれも「今の値を写して、あとで戻す」という同じことをしています。

* オブジェクトの複製
* プレファブへの保存と復元
* **コンポーネント削除の取り消し**

そこで経路をJSON1本にまとめました。
付け直すだけでは既定値のコンポーネントになってしまうため、
外す直前の値を写しておき、取り消したときに位置や表示状態まで元通りにします。

検証では、Meshコンポーネントを外して取り消すと元通り付き直ることを確認しました。

### 3-3. 画面

インスペクタは、見出し・折りたたみ・削除ボタンといった**どの型でも共通する枠だけ**を描き、
中身は `ComponentInspectors` の各関数に任せます。
そのため新しいコンポーネントが増えても `InspectorPanel` は変更不要です。

* `[コンポーネントを追加]` … 押すと登録済みの一覧が開く。すでに持っている型は選べない
* `[コンポーネントを削除]` … 各セクションの中。外せない型では**ボタン自体を出さない**

Transformを外せなくしているのは、描画コンポーネントがこれを前提にしているためです。

---

## 4. 名前の変更

3か所から行えます。

| 場所 | 操作 |
|---|---|
| インスペクタ | Name欄に打ち込んで確定 |
| ヒエラルキー | 右クリック →「名前を変更」 |
| ヒエラルキー | 選択して `F2` |

ヒエラルキーでは、その行だけが入力欄に差し替わります。
別のウィンドウを開くより、どれを変更しているのかが分かりやすいためです。

これを実現するために `InputTextOptions` へ `ActivateNow` を追加しました。
クリックを待たずに編集を始めるための指定で、メニューを選んだ直後から打ち込めます。
（「開始した最初の1フレームだけ立てる」ものである旨をコメントに明記しています）

名前が重複する場合は連番を足します。
このとき**自分自身は重複判定から外し**、変更前の自分と衝突して
`Player (1)` になってしまうのを防いでいます。

---

## 5. プレファブ機能は必要か → **必要。作成しました**

### 判断

今回の変更でオブジェクトは「作って、名前を付けて、コンポーネントを足して整える」
ものになりました。ここまで来ると、次に必ず

> 同じ構成のものをもう1体置きたい

が来ます。複製だけでは**エディタを閉じると消え、別のシーンでは使えません**。
組み立てた構成をファイルとして残せる仕組みは、この時点で入れておく価値があります。

一方で「置いたあとに設計図を直すと、置いてあるものにも反映される」という
UE5のような**連動までは持たせていません**。
その仕組みは実体側に「どの設計図から来たか」を持たせ、
差分を管理する必要があり、規模がまったく変わるためです。
今回は**置いた時点で切り離される**、素直な形にしています。

### つくり

保存先はコンテンツフォルダの `.prefab` ファイル（JSON）です。
`.effect` と形をそろえ、人が読めて差分も追える状態にしています。

```json
{
    "Format": "DX12FrameworkPrefab",
    "Version": 1,
    "Name": "Player",
    "Active": true,
    "Components": [
        { "Type": "TransformComponent", "Position": [1,2,3], "Rotation": [0,0,0], "Scale": [1,1,1] },
        { "Type": "MeshComponent" }
    ]
}
```

| ファイル | 担当 |
|---|---|
| `PrefabAsset` | 設計図そのものと、テキストとの相互変換 |
| `PrefabSystem` | GameObjectとの変換、ファイルの保存と読み込み |
| `ComponentRegistry` | コンポーネント1つぶんの値の写し取り |
| `AssetDatabase` | 実際のファイル作成（保存先フォルダの決定、連番付け） |

### 操作

| したいこと | 場所 |
|---|---|
| 保存 | インスペクタの `[プレファブとして保存]` / ヒエラルキー右クリック / 作成メニュー |
| 配置 | コンテンツブラウザでダブルクリック / 右クリック →「シーンへ配置」 / インスペクタ / 作成メニュー |

配置も**手で作った場合とまったく同じ経路**（`ObjectCommands::Spawn`）を通します。
そのため配置もそのままUndoで取り消せます。

知らない型のコンポーネントが入っていた場合は読み飛ばします。
保存したときより登録が減っていても、残りは復元できるようにするためです。

---

## 6. 確認結果

一連の操作を実際に実行し、状態を記録して確認しました。

| 確認項目 | 結果 |
|---|---|
| 作成 | `count 7 → 8` / 履歴 1件 |
| 名前の変更 | `UndoTest → Renamed` |
| コンポーネント追加（Mesh） | 付いた |
| Transform変更 | `Position.y = 2.0` |
| プレファブ保存 | `Assets/Content/Renamed.prefab` が作られた |
| コンポーネント削除 → 取り消し | 外れる → **元通り付き直る** |
| 削除 → 取り消し | `count 7 → 8` / **ID が 8 のまま変化なし** / 名前も保持 |
| すべて元に戻す | `count 7` / やり直せる操作 5件 |
| すべてやり直す | 削除まで含めて時系列どおり再現 |
| プレファブ配置 | 名前・`Position.y = 2.0`・Meshコンポーネントすべて復元 |

ショートカット（実際にキー入力を送って確認）

```
Ctrl+Z  … undo 6→5  count 8→7  （プレファブ配置を取り消し）
Ctrl+Z  … undo 5→4  count 7→8  （削除を取り消し＝オブジェクトが戻る）
Ctrl+Y  … undo 4→5  count 8→7  （削除をやり直し）
```

ウィンドウサイズの保存

| 確認項目 | 結果 |
|---|---|
| 1024×720 / 位置 120,90 で終了 → 再起動 | まったく同じ位置と大きさで開いた |
| 最大化して終了 → 再起動 | 最大化状態（クライアント 2560×1333）で開いた |
| 保存された「元に戻したときの大きさ」 | 1024×720 を維持（最大化サイズで上書きされていない） |

その他

* D3D12 デバッグレイヤーのメッセージ : **0**
* ウィンドウを閉じたときの終了 : 正常（ハングなし）
* ビルド : エラー・警告ともに **0**

---

## 7. プロジェクト構成

フォルダは崩していません。新規追加は次の5フォルダです。

```
Source/Application/WindowSettings/
Source/Editor/Commands/CommandHistory/
Source/Editor/Commands/ObjectCommands/
Source/Editor/Commands/ComponentCommands/
Source/Editor/Components/ComponentRegistry/
Source/Editor/Components/ComponentInspectors/
Source/Editor/Prefab/PrefabAsset/
Source/Editor/Prefab/PrefabSystem/
```

`Tools/SyncProjectFiles.py` を実行し、Visual Studio 側のフィルタへ反映しました。
`.vcxproj` / `.filters` の差分は**追加のみ 101行、削除 0行**です。
Shaderを含む既存のフィルタ、GUID、`Pch.cpp` の設定はすべて維持されています。

```bash
python Tools/SyncProjectFiles.py --check
```

### 変更ファイル一覧

新規

| ファイル | 内容 |
|---|---|
| `Editor/Commands/IEditorCommand.h` | 取り消せる操作のインターフェース |
| `Editor/Commands/CommandHistory/` | 履歴（2つのスタック） |
| `Editor/Commands/ObjectCommands/` | 作成・複製・配置・削除・名前変更・表示切替・Transform変更 |
| `Editor/Commands/ComponentCommands/` | コンポーネントの付け外し |
| `Editor/Components/ComponentRegistry/` | 型ごとの手順の登録表 |
| `Editor/Components/ComponentInspectors/` | 型ごとの画面表示 |
| `Editor/Prefab/PrefabAsset/` | 設計図の構造とJSON変換 |
| `Editor/Prefab/PrefabSystem/` | GameObjectとの変換、保存・読み込み・配置 |
| `Application/WindowSettings/` | ウィンドウ配置の保存 |

変更

| ファイル | 内容 |
|---|---|
| `Engine/GameObject/GameObjectManager` | `Detach()` を追加 |
| `Engine/GameObject/Components/MeshComponent` | `HasMesh()` を追加（表示フラグとメッシュ有無を区別するため） |
| `Engine/GameObject/Components/MeshletComponent` | `GetMeshCount()` を追加 |
| `Engine/EditorUI/Core/Input.h` | ショートカット用のキー（D / Y / Z / F2）を追加 |
| `Engine/EditorUI/Widgets` | `InputTextOptions::ActivateNow` を追加 |
| `Editor/Scene/GameObjectFactory` | 生の操作（生成・出し入れ）に専念する形へ整理 |
| `Editor/EditorContext.h` | `pHistory` を追加 |
| `Editor/EditorApp` | 履歴の所有、編集メニュー、ショートカット、シーン切替時の履歴破棄 |
| `Editor/Panels/InspectorPanel` | コンポーネントの追加・削除、履歴経由の編集 |
| `Editor/Panels/HierarchyPanel` | 行内での名前変更、履歴経由の操作、プレファブ保存 |
| `Editor/Panels/ContentBrowserPanel` | プレファブを開く＝シーンへ配置 |
| `Editor/Assets/AssetDatabase` | `.prefab` の種別を追加 |
| `Application/Application` | ウィンドウ配置の復元と保存、キー割り当ての追加 |
| `.gitignore` | `Config/` を追跡対象外に |

---

## 8. 残っている課題

今回の範囲外として、意図的に踏み込んでいない点です。

| 項目 | 内容 |
|---|---|
| プレファブの連動 | 設計図を直しても、すでに置いてあるものには反映されません。実体側に「どの設計図から来たか」と差分を持たせる必要があります |
| 複数選択 | 選択は常に1つです。まとめて削除・移動するには `Selection` を複数対応にし、コマンドも複数対象を扱う形にします |
| 親子関係 | 「グループ」は現状ただの目印です。実際に子を持たせるにはTransformに親子の概念が必要です |
| Undoの粒度 | シーンの切り替えや、コンテンツブラウザでのファイル操作は履歴に含みません |
| コンポーネントの登録 | Transform / Mesh / Meshlet の3種類のみです。増やす場合は `ComponentRegistry.cpp` の表へ1項目足します |
| メッシュの割り当て | エディタから作れるのは「まだメッシュが入っていない入れ物」までです。モデルの割り当てはシーン側が行います |
