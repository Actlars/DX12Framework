# エディタ改善と GPU パーティクル導入 変更まとめ（第2回）

対象 : DX12Framework / EditorUI・Editor・Engine::Effect・Renderer・RHI
ビルド : Debug x64 でエラー・警告 0。起動して D3D12 デバッグレイヤーのエラーも 0。

---

## 1. ご指摘いただいた不具合の対応

### 1-1. 文字がぼやける（ImGui風／Unity風に見える）

にじみの原因は 2 つあり、両方を直しました。

| 原因 | 対応 |
|---|---|
| **送り幅が小数** だったため、文字ごとに描画位置が半ピクセルずれ、リニア補間で輪郭が溶けていた | 送り幅を整数へ丸め、描画開始位置も `floor(x + 0.5)` でピクセル境界へ吸着 |
| **ClearType + NATURAL** でラスタライズしていたため、サブピクセルの色にじみがアルファへ混ざり、ヒンティングも効いていなかった | `CreateCustomRenderingParams` で **ClearTypeLevel = 0（グレースケール）+ GDI_CLASSIC + EnhancedContrast 0.7** に変更 |

GDI_CLASSIC は縦棒をピクセル境界へ吸着させるヒンティングで、UE5 のエディタ文字が
小さいサイズでもくっきり見えるのと同じ考え方です。
あわせて計測モードも `DWRITE_MEASURING_MODE_GDI_CLASSIC` にそろえました
（ラスタライズと計測がずれるとヒンティングの意味がなくなるため）。

送り幅より広い絵を持つグリフ（`W` など）が切れないよう、
セルの左右に `kGlyphOverhang` の余白を設け、`Bearing.x` でその分を戻しています。

- [Font.cpp](Source/Engine/EditorUI/Text/Font/Font.cpp)
- [DrawList.cpp](Source/Engine/EditorUI/Core/DrawList/DrawList.cpp)

### 1-2. フリーカメラが少し動くと固まる

**原因** : カーソルの再中央化を 2 か所で行っていました。

- `MouseInput`（相対モード）が毎フレーム **クライアント中心** へ戻す
- `GameScene::UpdateInput` が独自に **ウィンドウ中心** へ戻す

2 つが打ち消し合い、移動量がほぼ 0 になっていました。

**対応** : カーソル操作は `MouseInput` の責務に一本化し、
シーンは `GetMouseInput().GetDelta()` を使うだけにしました。
移動キーも `GetAsyncKeyState` から `KeyboardInput` へ移し、
他アプリ操作中にカメラが動かないようにしています。

- [GameScene.cpp](Source/Game/Scene/GameScene/GameScene.cpp)

### 1-3. ビューポートをドッキングから外すと、特定のサイズで固定される

**原因** : リサイズグリップを掴んだ瞬間に **カメラ操作が起動** していました。
相対モードに入るとカーソルが画面中心へ固定されるため、
「掴んだ位置からの移動量」が常に一定になり、サイズが変わらなくなります。
ビューポートだけで起きていたのは、カメラ操作の対象がビューポートだけだからです。

**対応** :

1. `Context::IsUiOperationActive()` を追加（ウィジェット編集・ウィンドウ移動／リサイズ／スクロール・ドッキング・ポップアップのいずれか）
2. `ViewportPanel` は、UI 操作中とフローティング時の右下グリップ領域を **カメラ操作の対象から除外**
3. カメラが操作権を持っている間は、UI 側へマウス座標を渡さない（画面中央の誤反応を防ぐ）

あわせて **リサイズ中の GPU ストール** も解消しました。
以前は 1 ピクセル動くたびにレンダーターゲットを作り直し、
そのたびに `WaitForGPU` が走っていました。

- 確保サイズを **128 ピクセル単位へ切り上げ**、収まっている間は作り直さない
- 表示は UV の部分矩形で切り出すため、見た目は 1 ピクセル単位で正確に追従する
- 大きく余ったときだけ縮めてメモリを解放する

- [ViewportTarget.{h,cpp}](Source/Engine/Renderer/ViewportTarget/ViewportTarget.h)
- [ViewportPanel.cpp](Source/Editor/Panels/ViewportPanel/ViewportPanel.cpp)

### 1-4. 「ウィンドウ」「ツール」がドッキング領域と重なる

**原因** : メニューバーの高さ (26) より、中身（余白 8 + ボタン 20 = 28）のほうが高かったためです。

**対応** : 高さを個別に決め打ちせず、`ボタンの高さ + 上下の余白` から算出するようにしました。
さらに `Context::SetNextWindowPadding()` を追加し、
このウィンドウだけ余白を小さくできるようにしています。

### 1-5. コンテンツブラウザがエクスプローラーと連動しない

**対応** : `DirectoryWatcher` を新設しました。

- `FindFirstChangeNotification` による **OS の通知** を使用
- 待機はタイムアウト 0 なので専用スレッド不要、変化がないときのコストはほぼゼロ
- コンテンツフォルダ全体を購読し、外部での作成・削除・リネームが次のフレームに反映される

`AssetDatabase::Update()` が通知を受けて読み直し、
コンテンツブラウザは描画の**前**にこれを呼びます
（描画中に一覧が入れ替わると走査中の参照が壊れるため）。

- [DirectoryWatcher.{h,cpp}](Source/Editor/Assets/DirectoryWatcher/DirectoryWatcher.h)

### 1-6. Visual Studio のフォルダ構成が崩れる

**対応** : `Tools/SyncProjectFiles.py` を用意しました。

```bash
python Tools/SyncProjectFiles.py
```

- `Source/` を走査し、`.vcxproj` の ClInclude / ClCompile を作り直す
- `.filters` を **ディスクのフォルダ構成そのまま** に生成する（ずれようがない）
- 既存のフィルタ GUID を引き継ぐ（VS がツリーの展開状態を忘れない）
- `Pch.cpp` の PrecompiledHeader などファイル固有の設定は保持する
- SlangTask / CustomBuild の ItemGroup には一切触れない
- BOM 付き UTF-8・CRLF のまま書き戻す
- **プロジェクトから外れるファイルがあれば必ず一覧表示する**（安全弁）

`--check` を付けると差分の有無だけを表示します。
現在、195 ファイルすべてがフォルダ構成と 1 対 1 で一致しています。

---

## 2. GPU パーティクル

CPU シミュレーションを廃し、発生・更新・描画をすべて GPU 上で行う方式へ置き換えました。
CPU が毎フレーム行うのは **定数バッファを 1 つ書くこと** だけで、
粒の位置や生死を CPU へ読み戻す処理は一度もありません。

### 2-1. パイプライン

```
[Emit CS]      未使用スロットを取り出して初期化
     ↓ UAVバリア
[Simulate CS]  全スロットを進め、生存リスト／未使用リストを作り直す
     ↓ UAVバリア
[Finalize CS]  生存数から間接描画の引数を作り、カウンタを戻す
     ↓
[DrawInstancedIndirect]  ビルボードをまとめて描画
```

描画数は GPU が決めるため、CPU は件数を知らないまま 1 命令で発行できます。
**GPU → CPU の同期待ちが発生しない**のがこの構成の要点です。

### 2-2. Wave intrinsics による最適化

素直に書くと、粒 1 つにつき 1 回 `InterlockedAdd` が必要になります。
プールが数万個あると、同一アドレスへの原子加算だけで処理時間が決まってしまいます。

そこで **波（wave）単位でまとめて確保** する形にしました。

```hlsl
// 波の中で「確保したいレーン」を数える
const uint laneCount  = WaveActiveCountBits(isAlive);   // この波で何個必要か
const uint laneOffset = WavePrefixCountBits(isAlive);   // 波の中で自分は何番目か

uint writeBase = 0;
if (WaveIsFirstLane())                                  // 代表レーンが1回だけ
{
    g_Counters.InterlockedAdd(COUNTER_OFFSET_ALIVE, laneCount, writeBase);
}
writeBase = WaveReadLaneFirst(writeBase);               // 結果を波全体へ配る

if (isAlive)
{
    g_AliveList[writeBase + laneOffset] = index;        // 重複しない位置が決まる
}
```

原子加算の回数が **波幅ぶんの 1（NVIDIA なら 32 分の 1）** に減ります。
Emit の未使用スロット確保、Simulate の生存登録とスロット返却の
3 か所すべてに同じ手法を適用しています。

### 2-3. その他の最適化

| 項目 | 内容 |
|---|---|
| 粒 1 つ 48 バイト | 色と大きさは粒ごとに持たず、エミッタ共通のパラメータから毎フレーム求める。帯域に直結するため最小限に絞った |
| ルートディスクリプタ | SRV/UAV をディスクリプタテーブルではなくルート直結にし、ビュー生成とバインドの手間をなくした |
| 頂点バッファなし | ビルボードは `SV_VertexID` から組み立てる。CPU から送るのは定数バッファのみ |
| 発生数 0 のフレーム | Emit のディスパッチ自体を省く |
| プールサイズ | スレッドグループの倍数へ切り上げ、シェーダー側の境界確認を減らす |
| 乱数 | PCG ハッシュ。1 回の乗算とシフトで十分に散らばる |

### 2-4. ファイル構成

```
Source/Engine/Effect/                          ← エンジン層（実行基盤）
  ParticleTypes.h                              CPU/GPU 共有の構造体と定数
  GpuParticleSystem/GpuParticleSystem.{h,cpp}  バッファ・PSO・ディスパッチ

Source/Engine/Shader/ParticleShader/
  ParticleCommon.hlsli          共有の型とユーティリティ（乱数・方向生成）
  ParticleEmitterParams.hlsli   発生・更新用の定数バッファ
  ParticleEmit.slang            CS : 発生
  ParticleSimulate.slang        CS : 更新 + リスト構築
  ParticleFinalize.slang        CS : 間接描画引数 + カウンタリセット
  ParticleVS.slang / ParticlePS.slang   ビルボード描画

Assets/Config/Json/RootSignature/GpuParticleCompute.json / GpuParticleRender.json
Assets/Config/Json/PipelineState/ParticleEmit.json / Simulate / Finalize / Render
```

シェーダーモデルは **6.9**（Slang.props の既定値）でコンパイルしています。

**定数バッファを共通ヘッダに置かない** ようにしている点だけ補足します。
発生・更新用と描画用はどちらも `b0` を使うため、共通ヘッダに置くと
描画シェーダー側で `b0` の二重宣言になります（Slang が警告→エラー）。
必要な側だけが `ParticleEmitterParams.hlsli` を取り込む形にしました。

### 2-5. エフェクトエディタ

`EffectEditorPanel` は **専用のレンダーターゲットと GPU パーティクル** を自分で持ちます。

- パネルを開くまで VRAM を消費しない（遅延確保）
- プレビュー内をドラッグでカメラ回転、ホイールで寄り引き
- 最大数を変えるとプールごと作り直す（バッファの大きさは生成時に決まるため）

そのために `IEditorPanel::OnRender()` を追加しました。
`OnGUI` が「何をどこに置くか」を決めるのに対し、`OnRender` は
「自分専用のレンダーターゲットへ実際に描く」ためのフックです。
`PanelManager::RenderAll()` → `EditorApp::RenderPanels()` → `Application::Tick` の順で呼ばれます。

CPU 実装だった `Editor/Effect/EffectPreview.{h,cpp}` は削除しました（重複を残さないため）。

**単位系を 3D へ統一** : `EffectAsset` はピクセルではなくワールド空間（メートル / 秒）で持ちます。
編集しやすい形（角度は度、重力は下向きの大きさ）で保持し、
シェーダーが読む形への変換は `EffectEditorPanel::BuildEmitterParams` が一手に引き受けます。

---

## 3. 責務分けの整理

| 追加した境界 | 目的 |
|---|---|
| `Context::IsUiOperationActive()` | 「UI がポインタを握っているか」の判断を 1 か所に集約。カメラ側が個別に判定しなくてよくなる |
| `IEditorPanel::OnRender()` | パネルの「レイアウト」と「GPU 描画」を分離 |
| `DirectoryWatcher` | 「変わったか」の検知と「どう読み直すか」を分離 |
| `WindowFrame::Padding` | 余白をスタイル固定ではなくウィンドウ単位に。レイアウト計算もフレームの値を見る |
| `GpuParticleSystem` / `EffectAsset` / `EffectEditorPanel` | 実行基盤 / データ / UI の三分割 |

---

## 4. デバッグ環境の改善

原因追跡に時間がかかったため、次を常設しました。

- `RHI::Device::FlushDebugMessages()` : D3D12 デバッグレイヤーのメッセージをログへ出す
- `CommandList::Reset` / `Renderer::EndFrame` : 失敗時に HRESULT を記録
  （`Close()` の失敗を見逃すとコマンドリストが記録中のまま残り、以降すべてのフレームが死にます）
- `Logger` : 毎回 `fflush`。異常終了時でも直前のログが残る

---

## 5. 変更ファイル一覧

### 新規

```
Source/Engine/Effect/ParticleTypes.h
Source/Engine/Effect/GpuParticleSystem/GpuParticleSystem.{h,cpp}
Source/Engine/Shader/ParticleShader/*.slang, *.hlsli
Source/Editor/Assets/DirectoryWatcher/DirectoryWatcher.{h,cpp}
Tools/SyncProjectFiles.py
Assets/Config/Json/{RootSignature,PipelineState}/...Particle...json
```

### 削除

```
Source/Editor/Effect/EffectPreview.{h,cpp}   （GPU実装へ移行したため）
```

### 主な変更

| ファイル | 内容 |
|---|---|
| `EditorUI/Text/Font/Font.cpp` | グレースケール + GDI ヒンティング、送り幅の整数化 |
| `EditorUI/Core/DrawList/DrawList.cpp` | 文字位置のピクセル吸着 |
| `EditorUI/Core/Context/Context.{h,cpp}` | `IsUiOperationActive` / `SetNextWindowPadding` |
| `EditorUI/Core/Window/WindowFrame.h` | ウィンドウ単位の余白 |
| `EditorUI/Widgets/Layout/Layout.h` | 幅計算をフレームの余白基準へ |
| `Renderer/ViewportTarget/*` | 確保サイズの粒度化、表示サイズとの分離 |
| `Editor/Panels/ViewportPanel/*` | カメラ操作の対象範囲を限定 |
| `Editor/Panels/EffectEditorPanel/*` | GPU パーティクル対応へ全面書き換え |
| `Editor/EditorApp/*`、`Editor/EditorContext.h` | デバイス共有、パネル描画フック |
| `Game/Scene/GameScene/GameScene.cpp` | カメラ入力の一本化 |
| `RHI/Core/{Device,CommandList}` | デバッグメッセージの取り出し |

---

## 6. 残っている課題

- **エフェクトはプレビュー専用**です。シーン中のオブジェクトへ貼り付けて再生する仕組み
  （`ParticleComponent` 相当）はまだありません。`GpuParticleSystem` はレンダーターゲットに依存しない
  作りなので、シーンの描画パスから `Render()` を呼べばそのまま使えます。
- **エミッタは 1 基のみ**です。Niagara のような複数エミッタや、モジュールの積み重ねには対応していません。
- **粒はテクスチャなし**の円形です。テクスチャ対応はピクセルシェーダーの差し替えで足せます。
- ヒエラルキーは平坦な一覧のままです（`GameObject` に親子関係がないため）。
- アセットのリネーム UI は未実装です（`AssetDatabase::Rename` は用意済み）。
