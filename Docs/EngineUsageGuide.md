# DX12Framework 使い方ガイド

このエンジンで実際に何かを作るときの手順書です。
仕組みの説明は [Architecture_Overview.md](Architecture_Overview.md) にあります。

---

## 1. ビルドと起動

| 項目 | 内容 |
|---|---|
| ソリューション | `DX12Framework.sln` |
| 構成 | `Debug` / `x64` |
| 実行ファイル | `x64/Debug/DX12Framework.exe` |
| 作業フォルダ | プロジェクト直下（`Assets` を相対パスで参照するため） |

シェーダーは `.slang` をビルド時に自動でコンパイルします。追加の手順はありません。

### ファイルを追加したら

`Source/` 以下に `.h` / `.cpp` を足したら、必ず次を実行します。

```bash
python Tools/SyncProjectFiles.py
```

Visual Studio のフィルタ（フォルダ表示）を、エクスプローラーの構成と一致させるツールです。
`--check` を付けると差分の有無だけを確認できます。手で `.vcxproj` を編集する必要はありません。

---

## 2. エディタの操作

### 画面の構成

```
┌──────────────────────────────────────────────┐
│ 作成  編集  シーン  ウィンドウ  ツール          │ ← メニューバー
├──────────┬────────────────────┬──────────────┤
│Hierarchy │      Viewport      │  Inspector   │
│          ├────────────────────┤              │
│          │  Content Browser   │              │
└──────────┴────────────────────┴──────────────┘
```

パネルはタイトルバーをドラッグして自由に組み替えられます。
画面外へドラッグするとその辺へドッキングします。

### ショートカット

| キー | 動作 |
|---|---|
| `Ctrl + Z` | 元に戻す |
| `Ctrl + Y` / `Ctrl + Shift + Z` | やり直す |
| `Ctrl + D` | 選択オブジェクトを複製 |
| `Delete` | 選択オブジェクトを削除 |
| `F2` | 選択オブジェクトの名前を変更 |
| `W A S D` / `Q E` | カメラ移動（ビューポート上で右ドラッグ中） |
| `M` | メッシュレット表示のデバッグ切り替え |

文字を入力している最中はショートカットが効きません。
名前の編集中に `Ctrl+Z` を押しても、シーンではなく入力欄が対象になります。

---

## 3. オブジェクトを作る

### 作成する

**メニューバー →「作成」** か、**ヒエラルキーで右クリック**。

| 項目 | 中身 |
|---|---|
| 空のオブジェクト | Transform だけを持つ |
| グループ | 同上（名前だけ違う。まとめる目印） |
| スポーン地点 | 同上（出現位置の目印） |

3つとも中身は同じ「基底クラスの `GameObject`」です。
違いは名前だけで、役割はヒエラルキー上で見分けるための目印になります。

### 名前を変える

* インスペクタの `Name` 欄
* ヒエラルキーで右クリック →「名前を変更」
* ヒエラルキーで選択して `F2`

同じ名前があると自動で ` (1)` が付きます。

### コンポーネントを付ける・外す

インスペクタ下部の **「コンポーネントを追加」**。
すでに持っている型は選べません。

外すときは各セクションの中の **「コンポーネントを削除」**。
`Transform` には削除ボタンが出ません（描画コンポーネントが前提にしているため）。

---

## 4. モデルを割り当てる

**インスペクタ → Mesh（または Meshlet）→「モデルを選択」**

一覧には `Assets` フォルダ以下のモデルファイル（`.fbx` / `.obj` / `.gltf` / `.glb`）が並びます。
メニューを開くたびに走査し直すので、あとから足したモデルもすぐ出てきます。

### Part（パート番号）とは

1つのモデルファイルは、たいてい複数のメッシュに分かれています
（体・髪・服が別メッシュ、など）。
`MeshComponent` が描くのはそのうちの**1つ**なので、番号で選びます。

```
Elinyaa.fbx  →  Part 0, 1, 2, 3, 4, 5   （6つのメッシュ）
```

モデル全体を出したい場合は、パートの数だけオブジェクトを置くか、
`MeshletComponent` を使います（こちらはモデル全体をまとめて描きます）。

### 選んでもすぐ変わらない場合

選んだ瞬間は「どのモデルを描きたいか」が記録されるだけで、
実際に結び付くのは**次のフレーム**です。
インスペクタに「読み込み待ち」と出ている間は、まだ結び付いていません。

初めて使うモデルは読み込みが走るため、少し時間がかかります。
2回目以降は `ModelLibrary` が覚えているので待ち時間はありません。

---

## 5. プレファブ（オブジェクトの設計図）

同じ構成のオブジェクトを何度も作りたいときに使います。

### 保存する

* インスペクタの **「プレファブとして保存」**
* ヒエラルキーで右クリック → **「プレファブとして保存」**
* メニューバー →「作成」→ **「選択オブジェクトをプレファブとして保存」**

コンテンツブラウザで**今開いているフォルダ**へ `.prefab` が作られます。

### 置く

* コンテンツブラウザで `.prefab` を**ダブルクリック**
* 右クリック → 「シーンへ配置」
* インスペクタの「シーンへ配置」

配置は `Ctrl+Z` で取り消せます。

### 保存される内容

```json
{
    "Format": "DX12FrameworkPrefab",
    "Name": "Player",
    "Active": true,
    "Components": [
        { "Type": "TransformComponent", "Position": [1,2,3], ... },
        { "Type": "MeshComponent", "Model": "Assets/Model/...", "Part": 2 }
    ]
}
```

JSON なので、テキストエディタで開いて手で直すこともできます。

**注意** : 置いたあとに設計図を直しても、すでに置いてあるものには反映されません
（置いた時点で切り離されます）。

---

## 6. シーンの保存と読み込み

**メニューバー →「シーン」**

| 項目 | 動作 |
|---|---|
| 上書き保存 | 今開いているシーンファイルへ保存（未保存のときは選べない） |
| 名前を付けて保存 | コンテンツブラウザの現在のフォルダへ `.scene` を作る |
| 選択シーンを開く | コンテンツブラウザで選んでいる `.scene` を開く |
| 新規シーン | すべて削除して空にする |

コンテンツブラウザで `.scene` をダブルクリックしても開けます。

### 開くときの注意

* **今置いてあるものはすべて消えます**（保存していない変更は戻せません）
* **Undo履歴も消えます** — オブジェクトが作り直されるため、
  それ以前の履歴は指す先を失います
* `MeshletComponent` を含むシーンは、開き直すときにモデルの
  メッシュレット分割をやり直すため数秒かかります

### 保存される内容

`GameObjectManager` に入っているオブジェクトと、そのコンポーネントの値だけです。
カメラの位置やポストエフェクトの設定は、現状 `GameScene` のコードが直接持っているため保存されません。

---

## 7. 新しいコンポーネントを作る

たとえば `RotatorComponent`（毎フレーム回転する）を足す場合の手順です。

### 手順1 : コンポーネント本体

`Source/Engine/GameObject/Components/RotatorComponent/RotatorComponent.h` / `.cpp`

```cpp
class RotatorComponent : public Component
{
public:
    void Update(float _deltaTime) override;

    void  SetSpeed(float _speed) { m_Speed = _speed; }
    float GetSpeed() const       { return m_Speed; }

private:
    float m_Speed = 90.0f;  // 1秒あたりの回転角(度)
};
```

これだけで `AddComponent<RotatorComponent>()` から使えます。

### 手順2 : エディタとファイルで扱えるようにする

`Source/Engine/GameObject/ComponentRegistry/ComponentRegistry.cpp` の表へ1項目足します。

```cpp
// 保存 / 復元
void SaveRotator(const GameObject& _object, nlohmann::json& _outJson)
{
    if (const auto* p = GetForRead<RotatorComponent>(_object))
    { _outJson["Speed"] = p->GetSpeed(); }
}

void LoadRotator(GameObject& _object, const nlohmann::json& _json)
{
    if (auto* p = _object.GetComponent<RotatorComponent>())
    { p->SetSpeed(_json.value("Speed", 90.0f)); }   // キーが無ければ既定値
}

// 表への登録
{
    "RotatorComponent",
    &HasComponent<RotatorComponent>,
    &AddComponent<RotatorComponent>,
    &RemoveComponent<RotatorComponent>,
    &SaveRotator,
    &LoadRotator,
},
```

**この時点で** 複製・プレファブ・シーンの保存・Undo がすべて対応します。

### 手順3 : インスペクタでの見せ方（任意）

`Source/Editor/Components/ComponentInspectors/ComponentInspectors.cpp` の表へ1項目足します。

```cpp
{ "RotatorComponent", "Rotator", true, &DrawRotator },
```

登録しなくても、型名がそのまま表示名になって扱えます。
日本語の表示名を付けたい、専用のUIを出したい、というときだけ足します。

### 手順4 : ファイルをプロジェクトへ反映

```bash
python Tools/SyncProjectFiles.py
```

---

## 8. 新しいシーンを作る

`IScene` を実装したクラスを `Source/Game/Scene/` へ作り、`SceneManager` へ渡します。

```cpp
class TitleScene : public IScene
{
public:
    bool OnInit(RHI::Device* _pDevice, ModelLibrary* _pModels) override;
    void OnTerm() override;
    void OnUpdate(float _deltaTime) override;
    void OnRender(ID3D12GraphicsCommandList* _pCmd) override;

    GameObjectManager* GetObjectManager() override { return &m_Objects; }
};
```

```cpp
m_SceneManager.ChangeScene(std::make_unique<TitleScene>());
```

切り替えはフレーム末に行われます（描画中に切り替わらないようにするため）。

`GetObjectManager()` を返しておくと、そのシーンもエディタで編集できるようになります。

### 描画コンポーネントを使う場合

`GameScene::SyncRenderComponents()` と同じ処理が必要です。
コンポーネントは「どのモデルを描きたいか」しか持たないため、
定数バッファの用意とモデルの結び付けはシーンが行います。
`GameScene.cpp` の該当部分をそのまま真似るのがいちばん確実です。

---

## 9. 新しいエディタパネルを作る

`IEditorPanel` を実装して `PanelManager` へ登録します。

```cpp
class MyPanel : public IEditorPanel
{
public:
    const std::string& GetTitle() const override { return m_Title; }
    std::string_view GetTypeName() const override { return "My Panel"; }

    void OnGUI(EditorContext& _ctx) override
    {
        EditorUI::Text(*_ctx.pUI, *_ctx.pFont, "Hello");
    }

private:
    std::string m_Title = "My Panel";
};
```

`EditorApp::RegisterDefaultPanels()` へ1行足せば常設パネルになります。
同じ種類を複数開けるようにしたい場合は `RegisterPanelFactories()` にも足します。

### シーンを書き換えるときの決まり

パネルから直接 `GameObject` を書き換えてはいけません。
必ず `ObjectCommands` / `ComponentCommands` を通します。

```cpp
// ×  取り消せなくなる
pObject->SetName("New Name");

// ○  履歴に残り、Ctrl+Zで戻せる
ObjectCommands::Rename(_ctx, pObject, "New Name");
```

---

## 10. よく使うウィジェット

```cpp
EditorUI::Context& ui   = *_ctx.pUI;
EditorUI::Font&    font = *_ctx.pFont;

// 文字
EditorUI::Text(ui, font, "通常の文字");
EditorUI::TextMuted(ui, font, "補足的な文字");
EditorUI::TextWrapped(ui, font, longText);      // 折り返す

// 値の編集（ラベル + 入力欄が1行になる）
EditorUI::Property(ui, font, "Name",     &nameString);
EditorUI::Property(ui, font, "Active",   &activeBool);
EditorUI::Property(ui, font, "Position", &float3Value);

// ボタン・選択
if (EditorUI::Button(ui, "実行", font, { 120.0f, 24.0f })) { ... }
const auto it = EditorUI::Selectable(ui, font, "行", isSelected);
if (it.Clicked) { ... }

// 折りたたみ
if (EditorUI::CollapsingHeader(ui, font, "Transform")) { ... }

// メニュー
if (EditorUI::BeginPopup(ui, "MyMenu"))
{
    if (EditorUI::MenuItem(ui, font, "項目", enabled)) { ... }
    EditorUI::EndPopup(ui);
}

// 色（円の色相環で選べる）
EditorUI::ColorProperty(ui, font, "Color", &float4Color);
```

### Idの衝突に注意

EditorUI はウィジェットをラベルから作った Id で識別します。
同じウィンドウ内に同じラベルが2つあると、同じウィジェットとみなされます。

```cpp
// 対象ごとにスコープを分ける
ui.GetIdStack().PushPtr(pObject);
    ...
ui.GetIdStack().Pop();
```

---

## 11. 困ったときに

| 症状 | 確認するところ |
|---|---|
| モデルが表示されない | インスペクタの Mesh で「読み込み待ち」「パート番号が範囲外」と出ていないか |
| 追加したファイルがVSに出ない | `python Tools/SyncProjectFiles.py` を実行したか |
| Undoが効かない操作がある | パネルが `ObjectCommands` を通さず直接書き換えていないか |
| シーンを開いたら履歴が消えた | 仕様（オブジェクトが作り直されるため） |
| ウィンドウの位置が毎回同じ | `Config/Window.json` が書けているか（終了時に保存） |
| D3D12の警告が出る | 出力ウィンドウに `ELOG` が出る。`Device::FlushDebugMessages()` で一覧を吐ける |

### ログ

```cpp
ELOG("失敗した : %s", detail);   // ファイル名と行番号つき
DLOG("情報 : %d", value);        // Debugビルドのみ
```

出力は Visual Studio の「出力」ウィンドウと標準出力の両方へ出ます。
