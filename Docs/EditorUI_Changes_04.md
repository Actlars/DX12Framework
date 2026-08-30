# ウィンドウ拡縮対応とオブジェクト作成メニュー 変更まとめ（第4回）

対象 : Application / RHI::Device / Renderer::GraphicsView / Editor
ビルド : Debug x64 でエラー・警告 0。
起動して最大化・最小化・枠ドラッグによる拡縮を行っても、D3D12 デバッグレイヤーのメッセージは 0。

今回の依頼は次の 2 点。

1. EditorUI は拡縮できるが、**OS のウィンドウ自体が拡縮・最大化できない**
2. メニューバーから**新しいオブジェクトを作成**したい（基底の `GameObject` を使うこと）

---

## 1. ウィンドウが拡縮・最大化できない

### 原因

原因は 2 つあり、片方だけ直しても動きません。

**(1) ウィンドウスタイルにサイズ変更用のフラグが無かった**

```cpp
// 変更前
const auto style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
```

`WS_THICKFRAME`（掴んで伸ばせる枠）と `WS_MAXIMIZEBOX`（最大化ボタン）が無いため、
そもそも OS がサイズ変更を受け付けていませんでした。

**(2) 仮にスタイルを足しても、スワップチェインが元の大きさのままだった**

バックバッファはウィンドウ生成時の大きさで作られたきりで、
`WM_SIZE` を受けても作り直していませんでした。
このままスタイルだけ足すと、引き伸ばした部分が描画されない・
表示が拡大されてぼやけるといった状態になります。

### 対応

#### スタイルの変更 — `Application::InitWnd()`

```cpp
// 変更後
const auto style = WS_OVERLAPPEDWINDOW;
```

`WS_OVERLAPPEDWINDOW` は
`WS_CAPTION` / `WS_SYSMENU` / `WS_THICKFRAME` / `WS_MINIMIZEBOX` / `WS_MAXIMIZEBOX`
をまとめて含みます。これで枠ドラッグ・最大化・最小化がすべて有効になります。

#### スワップチェインの作り直し — `RHI::Device::Resize()`（新規）

DX12 の作法どおりの順序で作り直します。順序を守らないと確実に落ちます。

| 手順 | 内容 | 理由 |
|---|---|---|
| 1 | `WaitForGPU()` | GPU が使用中のバックバッファを解放しないため |
| 2 | 全カラーターゲット・深度ターゲットを解放 | `ResizeBuffers` は参照が残っていると失敗する |
| 3 | `ResourceStateTracker` から登録解除 | **重要**。次項で説明 |
| 4 | `IDXGISwapChain::ResizeBuffers()` | 実際の作り直し |
| 5 | カラー／深度ターゲットを再生成 | 新しい大きさで作る |
| 6 | `UpdateFrameIndex()` | 作り直すとインデックスが変わるため |

手順 3 が抜けると、第 1 回で調査した「黒画面のまま復帰しない」不具合が再発します。
解放したリソースのアドレスは、再生成時に**別のリソースへ再利用される**ことがあり、
状態追跡に古い登録が残っていると、誤った before ステートでバリアを張って
`CommandList::Close()` が `E_INVALIDARG` で失敗します。
「解放とセットで必ず登録解除する」ことを守っています。

早期 return で守っている条件は 3 つです。

* スワップチェイン未生成
* 幅か高さが 0（最小化時。この状態では作り直せない）
* 大きさが変わっていない（最大化直後などに同じサイズの `WM_SIZE` が来る）

#### 上位への伝播 — `GraphicsView::Resize()`（新規）

`Device::Resize()` を呼んだあと、`Renderer` のビューポート・シザー矩形を作り直します。
`Application` から `Device` を直接触らせず、
「DX12 の面倒はここで完結させる」という既存の役割分担を保っています。

#### 反映のタイミング — `Application::ApplyPendingResize()`（新規）

`WM_SIZE` はメッセージ処理の途中で届きます。
その場でバックバッファを作り直すと、描画中に描画先が消える可能性があります。

そこで **受け取る場所と反映する場所を分けました**。

```
WM_SIZE       →  新しい大きさを控えるだけ（m_PendingWidth / m_PendingHeight）
Tick() の先頭 →  ApplyPendingResize() が安全なタイミングで実際に作り直す
```

枠をドラッグしている間は `WM_SIZE` が毎ピクセル飛んできますが、
控えるのは最新の 1 件だけなので、1 フレームにつき作り直しは最大 1 回で済みます。

最小化は `SIZE_MINIMIZED` を見て `m_IsMinimized` を立て、
`Tick()` の先頭で早期 return します。
クライアント領域が 0×0 になるため、描画そのものを止めるのがいちばん安全です。

UI 側の画面サイズ（`Context::UpdateDockSpaceLayout`）も同じ場所で更新します。
これを忘れるとドッキング領域が古い大きさのまま取り残されます。

### 変更ファイル

| ファイル | 内容 |
|---|---|
| `Application/Application.h` / `.cpp` | `WS_OVERLAPPEDWINDOW` 化、`WM_SIZE` の予約、`ApplyPendingResize()`、最小化中の描画停止 |
| `Engine/RHI/Core/Device/Device.h` / `.cpp` | `Resize()` を追加 |
| `Engine/Renderer/GraphicsView/GraphicsView.h` / `.cpp` | `Resize()` を追加 |

### 確認結果

| 項目 | 結果 |
|---|---|
| `WS_THICKFRAME` / `WS_MAXIMIZEBOX` / `WS_MINIMIZEBOX` | すべて有効（style = `0x14CF0000`） |
| 最大化 | クライアント 960×540 → 1707×889 |
| 元に戻す | 1707×889 → 960×540 |
| 最小化 → 復帰 | 停止せず復帰 |
| 枠ドラッグ相当（800×600 / 1500×1000 / 640×480 / 1200×850） | すべて正常。落ちない |
| D3D12 デバッグレイヤー | メッセージ 0 |

---

## 2. メニューバーからのオブジェクト作成

### 設計方針 — なぜ Factory を分けたか

作成操作は**メニューバー**と**ヒエラルキーの右クリック**の 2 か所から行えます。
それぞれに手続きを書くと、片方だけ直したときに挙動が食い違います。

そこで「**何をするか**」を `GameObjectFactory` に一本化し、
各 UI は「**いつ呼ぶか**」だけを持つようにしました。

```
GameObjectManager   オブジェクトの所有と寿命            （エンジン側）
GameObjectFactory   エディタ操作としての作成・複製・削除（今回追加）
各パネル / メニュー  どの操作をどこに置くか
```

状態を持たないため、クラスではなく**関数の集まり（namespace）**にしています。
インスタンスを作る必要も、寿命を考える必要もありません。

### `Editor::GameObjectFactory`（新規）

`Source/Editor/Scene/GameObjectFactory/GameObjectFactory.h` / `.cpp`

| 関数 | 内容 |
|---|---|
| `CreateEmpty(ctx, baseName, position)` | 基底 `GameObject` を 1 つ作り、`TransformComponent` を付けて選択状態にする |
| `Duplicate(ctx, pSource)` | 名前と Transform を引き継いで複製する |
| `Destroy(ctx, pTarget)` | 削除を予約し、選択中だった場合は選択も外す |
| `MakeUniqueName(ctx, baseName)` | 同名があれば `New Object (1)` のように連番を足す |

ご要望どおり、作られるのは**基底クラスの `GameObject` そのもの**です。
派生クラスを増やさず、必要な機能は Component で足していく既存の設計に合わせています。
そのため「空のオブジェクト」「グループ」「スポーン地点」は
**名前が違うだけで中身は同じ**です。役割はヒエラルキー上で見分けるための目印です。

細かい点として、次の 2 つに注意しています。

* **作った直後に選択状態にする** — 続けてインスペクタで編集できるようにするため
* **削除はフレーム末に行う** — `Update` / `Draw` の途中でオブジェクトリストが変わらないよう、
  `GameObjectManager::Remove()` の遅延削除をそのまま利用しています

### メニューバーへの追加 — `EditorApp`

メニューバーの先頭に「作成」を追加しました（既存の「ウィンドウ」「ツール」の左）。

| 項目 | 動作 | 有効条件 |
|---|---|---|
| 空のオブジェクト | `New Object` を作成 | シーンがある |
| グループ | `Group` を作成 | シーンがある |
| スポーン地点 | `Spawn Point` を作成 | シーンがある |
| （区切り線） | | |
| 選択オブジェクトを複製 | 選択中を複製 | 何か選択中 |
| 選択オブジェクトを削除 | 選択中を削除 | 何か選択中 |

押せない項目はグレー表示にして、「押しても何も起きない」ことを見た目で示しています。

### ヒエラルキー右クリックの整理

`HierarchyPanel` の右クリックメニューに書かれていた作成・複製・削除の処理を、
そのまま `GameObjectFactory` の呼び出しに置き換えました。
重複していたコードと、使われなくなったローカル変数を削除しています。

**メニューバーとヒエラルキーが同じ関数を呼ぶため、挙動が食い違うことがありません。**

### 変更ファイル

| ファイル | 内容 |
|---|---|
| `Editor/Scene/GameObjectFactory/GameObjectFactory.h` / `.cpp` | 新規。作成・複製・削除・命名 |
| `Editor/EditorApp/EditorApp.h` / `.cpp` | 「作成」メニューの追加と `DrawCreateMenu()` |
| `Editor/Panels/HierarchyPanel/HierarchyPanel.cpp` | 重複処理を Factory 呼び出しへ置き換え |

### 確認結果

同一経路で `New Object` / `New Object` / `Group` / `Spawn Point` を作成したところ、
ヒエラルキーに

```
New Object
New Object (1)
Group
Spawn Point
```

が並び、`Objects (11)` へ増加。
インスペクタは最後に作った `Spawn Point`（ID : 11、Position 0,0,0 / Scale 1,1,1）を表示。
連番による重複回避と、作成直後の自動選択が期待どおり動作することを確認しました。

---

## 3. プロジェクト構成について

今回もフォルダ構成は崩していません。

* 新規追加は `Source/Editor/Scene/GameObjectFactory/` の 1 フォルダのみ
* `Tools/SyncProjectFiles.py` を実行し、Visual Studio 側のフィルタへ反映
* 実行結果は「変更なし」= エクスプローラーと `.vcxproj` / `.filters` が一致している状態

Shader を含む既存のフィルタ、GUID、`Pch.cpp` の設定はすべて維持されています。

```bash
python Tools/SyncProjectFiles.py --check
```

でいつでも差分の有無だけを確認できます。

---

## 4. 残っている課題

今回の範囲外として、意図的に踏み込んでいない点です。

| 項目 | 内容 |
|---|---|
| Component ごと複製する | 現在の `Duplicate` は名前と Transform のみ引き継ぎます。Component まで複製するには、各 Component にクローンの仕組みが必要です |
| 親子関係 | 「グループ」は現状ただの目印です。実際に子を持たせるには Transform に親子の概念を入れる必要があります |
| Undo / Redo | 作成・削除は取り消せません。導入するなら操作をコマンド化する形になります |
| ウィンドウ位置・サイズの保存 | 終了時のサイズを次回起動時に復元する機能は入れていません |
