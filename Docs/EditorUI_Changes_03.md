# エディタ不具合対応と機能追加 変更まとめ（第3回）

対象 : EditorUI / Editor / Engine::Effect / RHI / Tools
ビルド : Debug x64 でエラー・警告 0。起動して D3D12 デバッグレイヤーのメッセージも 0。
エフェクトエディタの「開く → 閉じる」を実行しても、停止せず動作し続けることを確認済み。

---

## 1. エフェクトエディタを閉じると固まる／メモリリークが出る

### 原因

GPU リソースを **GPU がまだ使っている最中に破棄していました**。

コマンドリストは CPU より 1〜2 フレーム遅れて実行されます。
パネルを閉じた瞬間に `GpuParticleSystem` のバッファを解放すると、
実行中のコマンドが消えたリソースを参照し、デバイスロストになります。
その結果、画面が固まったまま終了できず、終了時に大量のリーク報告が出ていました。

`ViewportTarget::Term()` は完了待ちをしていましたが、
メンバの破棄順の都合で `GpuParticleSystem` のほうが先に消えており、
そちらには待機がありませんでした。

### 対応

| 場所 | 内容 |
|---|---|
| `GpuParticleSystem::Term()` | 解放前に `WaitForGPU()` を追加 |
| `PanelManager::RemoveClosedPanels()` | 破棄対象のパネルがある場合、**1 回だけ**完了を待ってから破棄 |

待機は重い処理ですが、ウィンドウを閉じた瞬間にしか起きないため実用上の負担はありません。
パネル単位ではなく PanelManager 側でまとめて待つことで、
今後 GPU リソースを持つパネルが増えても同じ守りが自動的に効きます。

---

## 2. ClearRenderTargetView の値が一致しない警告

### 原因

RenderGraph の中間バッファ `SceneColor` を作るときの**最適化クリア値**が `(0, 0, 1, 1)` のまま、
実際のクリアは `(0.06, 0.07, 0.09, 1)` で行っていました。

### 対応

背景色を `kSceneClearColor` として 1 か所に定義し、
リソース生成時とクリア時の両方がそれを参照するようにしました。
値が二度と食い違わない形にしています。

あわせて、もう 1 つ出ていた
`Ignoring InitialState D3D12_RESOURCE_STATE_UNORDERED_ACCESS` という情報メッセージも解消しました。
D3D12 ではバッファは必ず `COMMON` で作られ、指定した初期状態は無視されます。
`RHI::Buffer` が `COMMON` を渡すようにし、リソース追跡表にも `COMMON` で登録するよう直しています
（実際と違う状態を登録すると、最初のバリアで誤った遷移元が使われます）。

**結果 : デバッグレイヤーのメッセージは 0 件になりました。**

---

## 3. ドッキング中に「ウィンドウ」「ツール」が押せない

### 原因

ドッキング領域の再計算の**順番**が誤っていました。

```
UpdateDockSpaceLayout(画面全体)   ← ドック領域が y=0 から始まる
NewFrame()                        ← ここでホバー判定！ メニューバーがドックに覆われている
BuildUI() → SetDockArea(メニュー下) ← 遅すぎる
```

ホバー判定の時点ではドック領域が画面全体だったため、
メニューバーの上にドッキング中のパネルが重なっていると判定され、
ボタンにマウスが届いていませんでした。

### 対応

「毎フレーム矩形を渡す」形をやめ、
**インセット（上端の除外量）を一度だけ登録する**形に変えました。

```cpp
m_pUI->SetDockAreaInsetTop(kMenuBarHeight);   // EditorApp::Init で 1 回だけ
```

以降は `Context` が画面サイズの変化に自動で追従し、
`UpdateDockSpaceLayout` の中でインセットを引いた矩形を計算します。
ホバー判定より前に必ず正しい矩形が用意されるため、順番のずれが起きません。

---

## 4. 同じウィンドウを複数開けるようにしたい

ヒエラルキー・インスペクタ・コンテンツブラウザを、**それぞれ最大 4 枚**まで開けるようにしました。
「ウィンドウ」メニューの下段に追加項目が並び、現在の枚数と上限が表示されます。

```
ウィンドウ
  * Hierarchy
  * Inspector
  ...
  ─────────────
  ヒエラルキーを追加        (1 / 4)
  インスペクタを追加        (2 / 4)
  コンテンツブラウザを追加  (1 / 4)   ← 上限に達すると灰色になる
```

### 仕組み

| 追加したもの | 役割 |
|---|---|
| `IEditorPanel::GetTypeName()` | 通し番号を含まない「種類名」。何枚開いているかを数えるのに使う |
| `PanelManager::CountOfType()` | 種類ごとの枚数（保留中のぶんも含む） |
| `EditorApp::PanelFactory` | 「種類名 / メニュー表示名 / 生成関数 / 上限」の 4 つ組 |

各パネルのコンストラクタが通し番号を受け取り、2 枚目以降は `Inspector 2` のようにタイトルへ番号を付けます。
タイトルは EditorUI がウィンドウを識別する Id の元になるため、重複させられないからです。
番号は「まだ使われていない最小の値」を選ぶので、途中の 1 枚を閉じても番号が飛びません。

新しく複数開けるパネルを足すときは、`RegisterPanelFactories()` へ 1 行加えるだけで済みます。

**ビューポートだけは 1 枚のまま**です。
1 枚につき専用のレンダーターゲットとシーンの再描画が必要になり、
他のパネルと同じ扱いにできないためです（詳細は「残っている課題」）。

---

## 5. 円形のカラーピッカー

`Engine/EditorUI/Widgets/ColorWidgets.cpp` を新設しました。

| ウィジェット | 内容 |
|---|---|
| `ColorButton` | 現在の色を示す四角。押すとピッカーが開く |
| `ColorPicker4` | 色相環 + 明度スライダ + 不透明度スライダ + RGBA 数値欄 |
| `ColorProperty` | 「ラベル + 色見本」の 1 行。押すとピッカーがポップアップで開く |

### 設計の要点

- **色相環** : 角度で色相、中心からの距離で彩度を選ぶ。64 分割の扇形を並べ、
  頂点カラーの補間でなめらかに繋いでいます。そのために
  `DrawList::AddTriangleGradient()`（頂点ごとに色を変えられる三角形）を追加しました。
- **明度を環に反映** : 環自体を現在の明度で描くため、「いま選んでいる明るさでの色」が見えます。
- **半透明の表現** : 色見本と不透明度スライダの下地に市松模様を敷き、
  背景の濃さに左右されずに透け具合が分かるようにしています。
- **色相の保持** : 彩度 0（白や黒）では色相が数学的に決まりません。
  覚えていないと白に近づけた瞬間に色相が 0 へ飛ぶため、
  `Context` に float の預け先（`GetStorageFloat` / `SetStorageFloat`）を追加して引き継いでいます。
- **数値でも指定可能** : 環では出しにくい正確な値（R = 0.5 ちょうど 等）のために RGBA 欄を併設。
- **色空間の知識を閉じ込める** : 呼び出し側は常に RGBA(0.0〜1.0)。
  HSV への変換は `ColorWidgets.cpp` の中だけで完結します。

エフェクトエディタの Start Color / End Color をこの `ColorProperty` に差し替えました。

---

## 6. Visual Studio から Shader フォルダが消える

### 原因

前回作成した `Tools/SyncProjectFiles.py` が **C++（ClInclude / ClCompile）しか見ていませんでした**。
`.filters` のフィルタ定義を C++ のファイル一覧だけから作り直していたため、
シェーダー（SlangTask）だけが使っていたフォルダの定義が消え、
参照だけが宙に浮いてツリーが壊れていました。

### 対応

ツールがすべての項目種別を扱うようにしました。

| 種別 | 一覧の決め方 | 理由 |
|---|---|---|
| `ClInclude` / `ClCompile` | **ディスクを走査** | Source 以下はすべてビルド対象でよい |
| `SlangTask` / `CustomBuild` / `FxCompile` / `None` | **.vcxproj の記述を尊重** | どれをビルドするかは意図的な選択で、プロファイル等の設定も項目ごとに違う |

フォルダ割り当てはどの種別も同じ規則（ディスクのパスそのまま）で行うため、
エクスプローラーと VS のツリーが常に一致します。

いま `.filters` には未定義のフィルタ参照が 0 件、
`ParticleShader` を含むすべてのシェーダーフォルダが正しく並んでいます。

```bash
python Tools/SyncProjectFiles.py          # 同期する
python Tools/SyncProjectFiles.py --check  # 差分の確認だけ
```

---

## 7. 変更ファイル

### 新規

```
Source/Engine/EditorUI/Widgets/ColorWidgets.cpp     カラーピッカー一式
```

### 主な変更

| ファイル | 内容 |
|---|---|
| `Engine/Effect/GpuParticleSystem/GpuParticleSystem.cpp` | 解放前の GPU 完了待ち、バッファ初期状態を COMMON へ |
| `Editor/Panels/PanelManager/PanelManager.{h,cpp}` | 破棄前の完了待ち、種類ごとの枚数カウント |
| `Editor/Panels/IEditorPanel.h` | `GetTypeName()` の追加 |
| `Editor/Panels/{Hierarchy,Inspector,ContentBrowser}Panel/*` | 通し番号つきタイトル、種類名 |
| `Editor/EditorApp/EditorApp.{h,cpp}` | パネルファクトリ、ウィンドウメニューの追加項目、インセット指定 |
| `Engine/EditorUI/Core/Context/Context.{h,cpp}` | `SetDockAreaInsetTop`、float ストレージ |
| `Engine/EditorUI/Core/DrawList/DrawList.{h,cpp}` | `AddTriangleGradient` |
| `Engine/EditorUI/Widgets/Widgets.h` | カラーウィジェットの宣言 |
| `Engine/Renderer/SceneRenderer/SceneRenderer.cpp` | クリア値を 1 か所に統一 |
| `Engine/RHI/Resource/Buffer/Buffer.cpp` | バッファは COMMON で生成 |
| `Editor/Panels/EffectEditorPanel/EffectEditorPanel.cpp` | 色を `ColorProperty` へ差し替え |
| `Tools/SyncProjectFiles.py` | シェーダー等すべての項目種別に対応 |

---

## 8. 残っている課題

正直に記しておきます。

- **ビューポートは 1 枚のまま**です。複数開くには、パネルごとに専用のレンダーターゲットを持たせ、
  シーンをその枚数だけ描き直す必要があります。`ViewportPanel` を
  `EffectEditorPanel` と同じ「`OnRender` で自前のターゲットへ描く」形にすれば実現できますが、
  描画コストがそのまま枚数倍になるため、今回は見送りました。
- **カラーピッカーに履歴・スポイトはありません**。よく使う色を保存する仕組みは未実装です。
- **エフェクトはプレビュー専用**のままです（シーンへの配置は未対応）。
- ヒエラルキーは平坦な一覧です（`GameObject` に親子関係がないため）。
- 今回の確認はスクリーンショットとログによるもので、
  マウス操作の自動テストまでは行えていません。実際に触っての確認をお願いします。
