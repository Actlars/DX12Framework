# エディタ機能拡張とリファクタリング 変更まとめ

対象 : DX12Framework / EditorUI・Editor・Renderer 層
ビルド : Debug x64 でエラー・警告なし。実行して全パネルの表示とゲーム画面の描画を確認済み。

---

## 1. 全体像

これまで `Application` が「ウィンドウ管理」と「エディタUIの中身」の両方を持っていたため、
パネルを1枚増やすたびに `Application.cpp` が膨らむ構造になっていました。

今回、**エディタの中身を `Source/Editor/` へ独立**させ、責務を次のように分けました。

| 層 | 担当 | 主なクラス |
|---|---|---|
| `Application` | ウィンドウとメインループだけ | `Application` |
| `Editor` | どんなパネルがあり、何を編集するか | `EditorApp` / `PanelManager` / 各パネル |
| `Engine/EditorUI` | ウィジェットとウィンドウの仕組み | `Context` / `DrawList` / `DockController` |
| `Engine/Renderer` | 描画先の管理 | `ViewportTarget` / `SceneOutput` |

依存の向きは `Application → Editor → EditorUI → RHI` の一方向のみです。
パネル同士は直接参照せず、共有の `EditorContext`（選択状態・アセット・パネル一覧）を介して連携します。

---

## 2. 不具合の修正

### 2-1. 文字がウィンドウからはみ出して描かれる（ご指摘の問題）

**原因** : `DrawList::PushClipRect` が親のクリップ矩形との積を取らず、渡された矩形をそのまま積んでいました。
コメントには「積を積む」と書かれていましたが、実装が伴っていませんでした。
そのため、ウィジェットが自分の矩形をクリップとして積んだ瞬間に、
`BeginWindow` が設定したウィンドウのクリップが無効化されていました。

**修正** : `Rect2D::Intersect` を追加し、`PushClipRect` が必ず現在のクリップとの積を積むようにしました。
これにより、どのウィジェットが何を積んでも、ウィンドウの外へは決して描かれません。

- `Engine/EditorUI/Core/Types.h` : `Rect2D::Intersect` / `IsValid` / `Center` を追加
- `Engine/EditorUI/Core/DrawList.cpp` : `PushClipRect` を積集合方式へ

### 2-2. 文字が赤く、背景が黒い四角で塗り潰される

**原因** : フォントアトラスが `R8_UNORM`（1チャンネル）だったため、
ピクセルシェーダーの `texColor * vertexColor` で G と B が 0 になり、
文字が赤くなり、さらにアルファが常に 1 のためグリフの余白が不透明な黒で塗られていました。

**修正** : アトラスを `R8G8B8A8_UNORM` にし、**RGB を白・被覆率をアルファ**へ格納するようにしました。
これで矩形と文字をまったく同じシェーダーで扱えます（シェーダーの変更は不要）。

- `Engine/EditorUI/Text/Font/Font.cpp`

### 2-3. ドッキングの境界線を掴んでも動かない

**原因** : `DockSpace::DragSplit` が新しい比率を計算していたものの、ノードへ書き戻していませんでした。

### 2-4. ドッキング先の判定がほぼ常に失敗する

**原因** : `DockSpace::FindLeafAt` で ChildA の判定に `else` が付いておらず、
ChildA に入っていた場合でも続く ChildB の判定で弾かれ、必ず `-1` が返っていました。

### 2-5. コマンドリストが1フレーム目で壊れて描画が止まる

ゲーム画面のレンダーターゲット化を進める中で発生した問題です。

**原因** : 解放済みリソースと同じアドレスに新しい深度バッファが確保され、
`ResourceStateTracker` に残っていた古い状態（RENDER_TARGET）が使われて、
遷移元の食い違ったバリアが発行されていました。
その結果 `CommandList::Close()` が失敗し、コマンドリストが記録中のまま残って
以降の `Reset()` がすべて失敗し続けていました。

**修正** :
- `ViewportTarget` がカラー／深度の**両方**を生成時に登録し、解放時に登録解除するようにした
- `Renderer::EndFrame` / `CommandList::Reset` が失敗時に HRESULT をログへ出すようにした
- `RHI::Device::FlushDebugMessages()` を追加し、D3D12 デバッグレイヤーのメッセージをログへ出せるようにした
- `Logger` が毎回 `fflush` するようにし、異常終了時でも直前のログが残るようにした

### 2-6. 色のチャンネル順が逆になっていた

頂点フォーマットが `R8G8B8A8_UNORM` のため、`0xAARRGGBB` の形で16進を書くと赤と青が入れ替わります。
実際、`ColorAxisX`（赤のつもり）は青く表示されていました。

**修正** : `MakeColor(R, G, B, A)` を追加し、`Style` の色をすべてこれ経由の指定に統一しました。

---

## 3. 追加した機能

### 3-1. 画面外へ運んだときの外周ドッキング（ご要望）

ウィンドウを画面の外までドラッグすると、**出た方向の外周に吸着**するようになりました。

- `DockSpace::SplitRoot()` : 画面全体を割って外周に新しい区画を作る
- `DockController::ComputeScreenEdgeZone()` : マウスがどの辺の外側にあるかを判定
- `DockController::ComputeDropTarget()` : プレビューと実際のドッキングで**同じ判定**を共有
  （「プレビューは出るのにドッキングされない」というズレを構造的に防ぐ）
- 画面際での誤爆を避けるため、`Style::ScreenEdgeDockMargin` だけ外へ出て初めて反応します

あわせて、**ドッキングのプレビューが見えない問題**も直しました。
プレビューは移動中ウィンドウの `DrawList` に積まれており、そのウィンドウの矩形でクリップされていました。
どのウィンドウにも属さない**オーバーレイ用 `DrawList`** を `FrameContext` に追加し、
最前面・クリップなしで描くようにしています。

### 3-2. ゲーム画面をウィンドウへレンダーターゲット出力（ご要望）

- `Engine/Renderer/ViewportTarget/` : オフスクリーンのカラー＋深度＋SRV を管理。パネルの大きさに追従して作り直す
- `Engine/Renderer/SceneOutput/SceneOutput.h` : 「シーンをどこへ描くか」を表す1つの指定
- `SceneRenderer::SetOutputTarget()` : バックバッファ固定だった出力先を差し替え可能に
  （`RenderGraph` のパスもポストエフェクトも、すべてこの指定だけを見るように統一）
- `IScene::SetSceneOutput()` → `GameScene` がカメラの縦横比もパネルに合わせて更新

結果として、ゲーム画面は**他のパネルとまったく同じ1枚のウィンドウ**になり、
ドッキングもリサイズも自由にできます。表示されていないフレームはシーンの描画自体を省きます。

### 3-3. UE5風のエディタ構成

起動時に次の既定レイアウトを自動で組みます（以降はユーザーの配置が優先）。

```
+----------------------------------------------+
| メニューバー（ウィンドウ / ツール）           |
+-----------+------------------------+---------+
| Hierarchy |       Viewport         |Inspector|
+-----------+------------------------+---------+
|              Content Browser                 |
+----------------------------------------------+
```

| パネル | 内容 |
|---|---|
| **Hierarchy** | 実際の `GameObjectManager` の中身を一覧表示。検索欄、選択、右クリックで作成／複製／表示切替／削除 |
| **Inspector** | 選択中の対象を表示。GameObject なら名前・Active・Transform を直接編集、アセットなら情報と「開く」 |
| **Content Browser** | `Assets/Content` を閲覧。パンくずリスト、種類別アイコン色、ダブルクリックで開く |
| **Viewport** | ゲーム画面。マウスがこの上にあるときだけカメラ操作が有効 |
| **Effect Editor** | Niagara風のエフェクト編集（下記） |

エディタ用の複製データは持たず、**実際のシーンをそのまま編集**します。

### 3-4. Niagara風のエフェクトエディタ（ご要望）

`.effect` ファイル1つにつき1枚のウィンドウが開きます。

- `Editor/Effect/EffectAsset.h` : エミッタのパラメータ一式（JSONで保存）
- `Editor/Effect/EffectPreview` : CPUパーティクルの簡易シミュレーション
- `Editor/Panels/EffectEditorPanel/` : 再生／一時停止／最初から／保存 ＋ プレビュー ＋ パラメータ

編集できる項目 : 発生数・形状（点/円/矩形）・寿命・初速・拡散角・重力・減衰・開始/終了サイズ・開始/終了カラー・ループ・最大数。
値を変えるとその場でプレビューに反映されます。

### 3-5. 右クリックメニュー（ご要望）

ポップアップは**「常に最前面に出る枠なしのウィンドウ」**として実装しました。
専用の描画経路を作らず既存のウィンドウ機構に乗せているため、
クリップ・入力の占有・Z順の扱いが本体と自動的に一致します。

- コンテンツブラウザの空白部 : 新しいフォルダ / 新しいテキストファイル / 新しいエフェクト / エフェクト（エディタで開く）/ 再読み込み
- コンテンツブラウザの項目上 : 開く / 削除 ＋ 上記の作成メニュー
- ヒエラルキー : 空のオブジェクトを作成 / 複製 / アクティブ切替 / 削除
- メニューバー「ツール」 : 新規エフェクトエディタ / コンテンツ再読み込み

### 3-6. 操作性の改善

- **ウィンドウのタイトルが表示されるようになった**（従来はタイトルバーが無地でした）
- **ドックタブに名前と × ボタンが付いた**。タブ幅は文字の長さに合わせて可変
- タイトルバーにも × ボタン（`bool*` を渡したウィンドウのみ）
- ダブルクリック判定を追加（コンテンツブラウザの「開く」に使用）
- メニューバーの下だけをドッキング領域にし、パネルがメニューを覆わないように
- バックバッファのクリア色を青から暗いグレーへ（パネルの隙間が目立たない）

### 3-7. 文字色（ご要望）

純白は暗い背景でにじみやすく、長時間の作業でまぶしいため、
**わずかに輝度を落とした、青みのあるオフホワイト**にしました。

| 用途 | 色 |
|---|---|
| 通常テキスト | `MakeColor(208, 211, 216)` |
| 見出し・強調 | `MakeColor(232, 234, 238)` |
| 補足 | `MakeColor(146, 150, 158)` |
| 無効 | `MakeColor(104, 108, 114)` |

---

## 4. 追加したウィジェット

一覧表示のための部品を `Engine/EditorUI/Widgets/ContainerWidgets.cpp` にまとめました。

| ウィジェット | 用途 |
|---|---|
| `Selectable` | 選択できる1行。左/右/ダブルクリックを `ItemInteraction` で返す |
| `TreeNode` / `TreePop` | 開閉できるツリー。開閉状態は `Context` のストレージが保持 |
| `CollapsingHeader` | 折り畳める見出し |
| `SeparatorText` | 見出し付きの区切り線 |
| `Image` | テクスチャの表示（ゲーム画面に使用） |
| `MenuItem` / `MenuSeparator` | メニュー項目 |
| `BeginPopupContextWindow` / `BeginPopupContextItem` | 右クリックメニューの開始条件 |
| `Indent` / `Unindent` / `Dummy` | レイアウト補助 |

---

## 5. ファイル構成

### 新規

```
Source/Editor/
  EditorContext.h                             パネル共有の参照束
  Selection/Selection.h                       選択状態
  Assets/AssetDatabase.{h,cpp}                ファイル木（UIを持たない）
  Effect/EffectAsset.{h,cpp}                  エフェクトのデータとJSON入出力
  Effect/EffectPreview.{h,cpp}                CPUパーティクル
  Panels/IEditorPanel.h                       パネルの最小の約束
  Panels/PanelManager/                        パネルの所有・描画・後片付け
  Panels/ViewportPanel/                       ゲーム画面
  Panels/HierarchyPanel/                      オブジェクト一覧
  Panels/InspectorPanel/                      詳細の表示と編集
  Panels/ContentBrowserPanel/                 アセット閲覧
  Panels/EffectEditorPanel/                   エフェクト編集
  EditorApp/                                  エディタ全体のとりまとめ

Source/Engine/Renderer/
  ViewportTarget/                             オフスクリーン描画先
  SceneOutput/SceneOutput.h                   描画先の指定

Source/Engine/EditorUI/Widgets/
  ContainerWidgets.cpp                        一覧・ツリー・メニュー・画像
```

### 主な変更

| ファイル | 変更点 |
|---|---|
| `Application/Application.{h,cpp}` | エディタの中身を `EditorApp` へ委譲。ウィンドウとループのみに |
| `EditorUI/Core/Context/Context.{h,cpp}` | ポップアップ、フォント共有、タイトル描画、レイアウト構築API |
| `EditorUI/Core/DrawList.{h,cpp}` | クリップの積集合化、`AddImage` |
| `EditorUI/Core/Style.h` | 配色を `MakeColor` で全面的に見直し |
| `EditorUI/Docking/DockSpace.{h,cpp}` | `SplitRoot`、2件のバグ修正 |
| `EditorUI/Docking/DockController.{h,cpp}` | 外周ドッキング、タブ名と×、判定の一本化 |
| `Renderer/SceneRenderer.{h,cpp}` | 出力先の差し替えに対応 |
| `PostProcess/*` | `SceneOutput` を受け取る形へ |
| `Scene/IScene.h`・`SceneManager`・`GameScene` | `SetSceneOutput` の追加 |
| `RHI/Core/Device`・`CommandList`・`ColorTarget` | 診断ログ、クリア値の指定 |

---

## 6. 残っている課題

正直に記しておきます。

- **ヒエラルキーは平坦な一覧です。** `GameObject` がまだ親子関係を持たないためで、
  階層が入り次第 `TreeNode` へ差し替えられるよう、行の描画は1か所にまとめてあります。
- **オブジェクトの複製は名前と Transform のみ**を引き継ぎます。
  完全な複製には Component のクローン機構が必要です。
- **エフェクトのプレビューは2Dのみ**です。3Dシーンへの反映は未実装です。
- **リネーム用のUIがありません。** `AssetDatabase::Rename` は用意済みなので、
  インライン編集を足せばすぐ繋がります。
- 右クリックメニューの動作は、コードレビューと実行時のクラッシュ確認までは行いましたが、
  **マウス操作の自動テストまでは行えていません**。実際に触っての確認をお願いします。
