// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "EffectEditorPanel.h"

#include <Engine/EditorUI/Widgets/Widgets.h>
#include <Engine/EditorUI/Widgets/Layout/Layout.h>
#include <Editor/Panels/PanelManager/PanelManager.h>
#include <Engine/Utility/Debug/Logger/Logger.h>

namespace
{
	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kDegreeToRadian = kPi / 180.0f;

	// -------------------------------------------------------------------------------
	// ウィンドウのタイトルはIdの元になるため、必ず一意にする
	// 「Effect: <ファイル名>」という形にすることで、
	// 同じファイルを二度開こうとしたときにFindで拾える
	// -------------------------------------------------------------------------------
	std::string MakeTitle(const std::filesystem::path& _path)
	{
		return "Effect: " + _path.filename().string();
	}

	// ファイルを持たないエディタ用に、通し番号でタイトルを作る
	std::string MakeScratchTitle()
	{
		static int s_Counter = 0;
		return "Effect Editor " + std::to_string(++s_Counter);
	}

	// エミッタ形状の選択肢
	constexpr std::string_view kShapeLabels[] = { "Point", "Sphere", "Box" };

	// プレビュー領域の高さの下限・上限
	constexpr float kPreviewMinHeight = 120.0f;
	constexpr float kPreviewMaxHeight = 420.0f;

	// 1フレームの経過時間の上限
	// デバッガで止めた直後などに、粒が一気に飛ぶのを防ぐ
	constexpr float kMaxDeltaTime = 1.0f / 20.0f;
}

// -------------------------------------------------------------------------------
// .effectファイルに対応するエディタを開く
// -------------------------------------------------------------------------------
Editor::EffectEditorPanel* Editor::EffectEditorPanel::OpenForAsset(
	EditorContext& _ctx, const std::filesystem::path& _path)
{
	if (_ctx.pPanels == nullptr)
	{ return nullptr; }

	const std::string title = MakeTitle(_path);

	// すでに開いていれば、それを手前に出すだけにする
	if (IEditorPanel* pExisting = _ctx.pPanels->Find(title))
	{
		pExisting->SetOpen(true);
		return static_cast<EffectEditorPanel*>(pExisting);
	}

	auto panel = std::make_unique<EffectEditorPanel>(title, _path);
	panel->LoadFromFile();

	return static_cast<EffectEditorPanel*>(_ctx.pPanels->Add(std::move(panel)));
}

// -------------------------------------------------------------------------------
// ファイルを持たないエディタを開く
// -------------------------------------------------------------------------------
Editor::EffectEditorPanel* Editor::EffectEditorPanel::OpenScratch(EditorContext& _ctx)
{
	if (_ctx.pPanels == nullptr)
	{ return nullptr; }

	auto panel = std::make_unique<EffectEditorPanel>(MakeScratchTitle(), std::filesystem::path{});

	return static_cast<EffectEditorPanel*>(_ctx.pPanels->Add(std::move(panel)));
}

Editor::EffectEditorPanel::EffectEditorPanel(std::string _title, std::filesystem::path _path)
	: m_Title(std::move(_title))
	, m_Path(std::move(_path))
{
	// プレビューの縦幅とパラメータの両方が要るため、縦に長めのウィンドウで開く
	SetInitialPlacement({ 200.0f, 120.0f }, { 400.0f, 560.0f });

	if (!m_Path.empty())
	{
		m_Effect.Name = m_Path.stem().string();
	}
}

Editor::EffectEditorPanel::~EffectEditorPanel()
{
	// GPUリソースはunique_ptrのデストラクタで解放される
	// ViewportTarget / GpuParticleSystem がそれぞれGPU完了待ちを行う
}

// -------------------------------------------------------------------------------
// エフェクトエディタの中身
// -------------------------------------------------------------------------------
void Editor::EffectEditorPanel::OnGUI(EditorContext& _ctx)
{
	// このフレームの要求をいったん無効にしておく
	// 折り畳まれた場合などにOnGUIが呼ばれず、古い値が残るのを防ぐ
	m_PreviewWidth	= 0;
	m_PreviewHeight	= 0;

	DrawToolbar(_ctx);

	DrawPreview(_ctx);
	DrawParameters(_ctx);
}

// -------------------------------------------------------------------------------
// 上段 : 再生の操作と保存
// -------------------------------------------------------------------------------
void Editor::EffectEditorPanel::DrawToolbar(EditorContext& _ctx)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	if (EditorUI::Button(ui, m_Playing ? "Pause" : "Play", font, { 64.0f, 24.0f }))
	{
		m_Playing = !m_Playing;
	}

	EditorUI::SameLine(ui);

	if (EditorUI::Button(ui, "Restart", font, { 72.0f, 24.0f }))
	{
		// 粒をすべて消して最初から
		if (m_pParticles != nullptr)
		{
			m_pParticles->RequestReset();
		}

		m_PlayTime			= 0.0f;
		m_SpawnAccumulator	= 0.0f;
		m_Playing			= true;
	}

	EditorUI::SameLine(ui);

	// ファイルに紐づいていない場合は保存先がないため、押せないことを見た目で示す
	const bool canSave = !m_Path.empty();

	if (EditorUI::Button(ui, canSave ? "Save" : "Save (no file)", font, { 90.0f, 24.0f }) && canSave)
	{
		if (SaveToFile())
		{
			m_Dirty = false;
		}
	}

	// 状態表示
	// 生存数はGPU側が持っていてCPUへは戻らないため、上限だけを示す
	const std::string status =
		std::string(m_Dirty ? "* 未保存   " : "保存済み   ") +
		"GPU Particles (max " + std::to_string(m_Effect.MaxParticles) + ")";

	EditorUI::TextMuted(ui, font, status);
}

// -------------------------------------------------------------------------------
// 中段 : プレビュー
//
// 場所取り(Dummy)で領域を確保し、前フレームに描かれたテクスチャを貼る
// 実際の描画はOnRenderで行う
// -------------------------------------------------------------------------------
void Editor::EffectEditorPanel::DrawPreview(EditorContext& _ctx)
{
	EditorUI::Context&		ui		= *_ctx.pUI;
	EditorUI::WindowFrame*	pFrame	= ui.GetCurrentWindow();

	if (pFrame == nullptr)
	{ return; }

	const EditorUI::Style& style = ui.GetStyle();

	// ウィンドウの高さに応じてプレビューの大きさを決める
	// 固定値にすると、小さくしたときにパラメータが一切見えなくなる
	const float width  = EditorUI::GetContentWidth(pFrame, style);
	const float height = std::clamp(
		pFrame->ContentRect.Height() * m_PreviewHeightRatio,
		kPreviewMinHeight,
		kPreviewMaxHeight);

	const EditorUI::Rect2D bounds = EditorUI::Dummy(ui, { width, height });

	if (!bounds.IsValid())
	{ return; }

	// 背景と枠。ここが「舞台」であることを分かるようにする
	pFrame->Draw.AddRectFilled(bounds, style.ColorPanelBg);
	pFrame->Draw.AddRectOutline(bounds, style.ColorBorderLight, style.BorderThickness);

	// このフレームに必要な描画サイズを記録する。OnRenderが読む
	m_PreviewWidth	= static_cast<uint32_t>(bounds.Width());
	m_PreviewHeight	= static_cast<uint32_t>(bounds.Height());

	// -------------------------------------------------------------------------------
	// プレビュー内のドラッグでカメラを回す
	//
	//	ウィンドウの移動と取り合わないよう、ここではUIの操作状態を見て判断する
	// -------------------------------------------------------------------------------
	if (bounds.Contains(ui.GetMousePos()) &&
		ui.IsCurrentWindowHovered() &&
		ui.IsMouseDown(EditorUI::MouseButton::Mouse_Left) &&
		!ui.IsUiOperationActive())
	{
		const DirectX::XMFLOAT2 delta = ui.GetMouseDelta();

		m_CameraYaw   += delta.x * 0.01f;
		m_CameraPitch  = std::clamp(m_CameraPitch + delta.y * 0.01f, -1.4f, 1.4f);
	}

	// ホイールで寄り引き
	if (bounds.Contains(ui.GetMousePos()) && ui.IsCurrentWindowHovered())
	{
		const float wheel = ui.GetMouseWheel();
		if (wheel != 0.0f)
		{
			m_CameraDistance = std::clamp(m_CameraDistance - wheel * 0.5f, 1.0f, 40.0f);
		}
	}

	// -------------------------------------------------------------------------------
	// 前フレームに描かれた絵を貼る
	// -------------------------------------------------------------------------------
	if (m_pPreviewTarget == nullptr || !m_pPreviewTarget->IsValid())
	{
		return;	// 初回はまだ用意できていない
	}

	// 確保サイズと表示サイズが違うため、表示ぶんだけを切り出す
	const float targetWidth  = static_cast<float>(m_pPreviewTarget->GetWidth());
	const float targetHeight = static_cast<float>(m_pPreviewTarget->GetHeight());

	EditorUI::Rect2D uv{ { 0.0f, 0.0f }, { 1.0f, 1.0f } };

	if (targetWidth > 0.0f && targetHeight > 0.0f)
	{
		uv.Max.x = (std::min)(1.0f, static_cast<float>(m_pPreviewTarget->GetViewWidth())  / targetWidth);
		uv.Max.y = (std::min)(1.0f, static_cast<float>(m_pPreviewTarget->GetViewHeight()) / targetHeight);
	}

	pFrame->Draw.AddImage(bounds, m_pPreviewTarget->GetTextureId(), uv);
}

// -------------------------------------------------------------------------------
// GPU描画
//
// UIの構築が終わったあとに呼ばれるため、
// このフレームのプレビューの大きさがそのまま使える
// -------------------------------------------------------------------------------
void Editor::EffectEditorPanel::OnRender(EditorContext& _ctx, ID3D12GraphicsCommandList* _pCmd)
{
	if (m_PreviewWidth == 0 || m_PreviewHeight == 0)
	{
		return;	// 表示されていない。描く必要がない
	}

	if (!EnsurePreviewResources(_ctx))
	{
		return;
	}

	if (!m_pPreviewTarget->Resize(m_PreviewWidth, m_PreviewHeight))
	{
		return;
	}

	// -------------------------------------------------------------------------------
	// 1. GPU上で発生と更新を行う
	//    描画先の設定に依存しないため、レンダーターゲットを切り替える前に済ませる
	// -------------------------------------------------------------------------------
	const float deltaTime = m_Playing
		? (std::min)(_ctx.DeltaTime, kMaxDeltaTime)
		: 0.0f;

	m_pParticles->Update(_pCmd, BuildEmitterParams(deltaTime));

	// -------------------------------------------------------------------------------
	// 2. プレビュー専用のレンダーターゲットへ描く
	// -------------------------------------------------------------------------------
	m_pPreviewTarget->Begin(_pCmd);
	m_pParticles->Render(_pCmd, BuildViewParams());
	m_pPreviewTarget->End(_pCmd);
}

// -------------------------------------------------------------------------------
// プレビュー用GPUリソースの遅延確保
// -------------------------------------------------------------------------------
bool Editor::EffectEditorPanel::EnsurePreviewResources(EditorContext& _ctx)
{
	if (_ctx.pDevice == nullptr || _ctx.pUIRenderer == nullptr)
	{
		return false;
	}

	if (m_pPreviewTarget == nullptr)
	{
		m_pPreviewTarget = std::make_unique<ViewportTarget>();

		if (!m_pPreviewTarget->Init(_ctx.pDevice, _ctx.pUIRenderer))
		{
			ELOG("EffectEditorPanel::EnsurePreviewResources() ViewportTarget::Init failed");
			m_pPreviewTarget.reset();
			return false;
		}
	}

	// -------------------------------------------------------------------------------
	// 最大数を変えた場合はプールごと作り直す
	// バッファの大きさは生成時に決まるため、途中で増減させられない
	// -------------------------------------------------------------------------------
	const uint32_t requestedMax = static_cast<uint32_t>((std::max)(1, m_Effect.MaxParticles));

	if (m_pParticles != nullptr && m_pParticles->GetMaxParticles() < requestedMax)
	{
		m_pParticles.reset();
	}

	if (m_pParticles == nullptr)
	{
		Effect::GpuParticleDesc desc;
		desc.MaxParticles = requestedMax;

		m_pParticles = std::make_unique<Effect::GpuParticleSystem>();

		if (!m_pParticles->Init(_ctx.pDevice, desc))
		{
			ELOG("EffectEditorPanel::EnsurePreviewResources() GpuParticleSystem::Init failed");
			m_pParticles.reset();
			return false;
		}
	}

	return true;
}

// -------------------------------------------------------------------------------
// このフレームの発生パラメータ
//
// EffectAsset（編集用の素直な値）を、シェーダーが読む形へ変換する
// 角度は度からラジアンへ、重力は下向きのベクトルへ、といった変換をここに集約する
// -------------------------------------------------------------------------------
Effect::EmitterParams Editor::EffectEditorPanel::BuildEmitterParams(float _deltaTime)
{
	Effect::EmitterParams params{};

	params.Origin		= { 0.0f, 0.0f, 0.0f };
	params.DeltaTime	= _deltaTime;

	// 重力は「下向きの大きさ」で編集させ、ここでベクトルに直す
	params.Gravity		= { 0.0f, -m_Effect.Gravity, 0.0f };
	params.Drag			= m_Effect.Drag;

	params.LifeTime				= m_Effect.LifeTime;
	params.LifeTimeRandom		= m_Effect.LifeTimeRandom;
	params.InitialSpeed			= m_Effect.InitialSpeed;
	params.InitialSpeedRandom	= m_Effect.InitialSpeedRandom;

	params.SpreadAngle	= m_Effect.SpreadAngle * kDegreeToRadian;
	params.ShapeRadius	= m_Effect.ShapeRadius;
	params.Shape		= static_cast<uint32_t>(m_Effect.Shape);

	// -------------------------------------------------------------------------------
	// このフレームに出す数
	//
	// 「1フレームあたり何粒」は小数になるため、端数を持ち越して総数を合わせる
	// 持ち越さないと、低いSpawnRateでは1粒も出ないフレームが続いてしまう
	// -------------------------------------------------------------------------------
	uint32_t spawnCount = 0;

	if (_deltaTime > 0.0f)
	{
		m_PlayTime += _deltaTime;

		// ループしない設定では、再生時間を過ぎたら発生を止める
		const bool spawning = m_Effect.Looping || (m_PlayTime <= m_Effect.Duration);

		if (spawning && m_Effect.SpawnRate > 0.0f)
		{
			m_SpawnAccumulator += m_Effect.SpawnRate * _deltaTime;

			spawnCount = static_cast<uint32_t>(m_SpawnAccumulator);
			m_SpawnAccumulator -= static_cast<float>(spawnCount);
		}
	}

	params.SpawnCount = spawnCount;

	return params;
}

// -------------------------------------------------------------------------------
// プレビューカメラ
//
// 原点を見下ろす軌道カメラ
// ビルボードを組み立てるための右方向・上方向もここで求めて渡す
// （シェーダー側でビュー行列から取り出すより、CPUで1回求めるほうが安い）
// -------------------------------------------------------------------------------
Effect::ParticleViewParams Editor::EffectEditorPanel::BuildViewParams() const
{
	using namespace DirectX;

	Effect::ParticleViewParams view{};

	const float cosPitch = std::cos(m_CameraPitch);

	const XMVECTOR eye = XMVectorSet(
		std::sin(m_CameraYaw) * cosPitch * m_CameraDistance,
		std::sin(m_CameraPitch) * m_CameraDistance,
		std::cos(m_CameraYaw) * cosPitch * m_CameraDistance,
		1.0f);

	// 粒は上方向へ噴き上がるため、少し上を注視点にすると全体が収まる
	const XMVECTOR target	= XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f);
	const XMVECTOR worldUp	= XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	const XMMATRIX viewMatrix = XMMatrixLookAtLH(eye, target, worldUp);

	const float aspect = (m_PreviewHeight > 0)
		? static_cast<float>(m_PreviewWidth) / static_cast<float>(m_PreviewHeight)
		: 1.0f;

	const XMMATRIX projection = XMMatrixPerspectiveFovLH(
		60.0f * kDegreeToRadian, aspect, 0.05f, 200.0f);

	XMStoreFloat4x4(&view.ViewProjection, XMMatrixTranspose(viewMatrix * projection));

	// ビュー行列の各列が、そのままカメラの基底になる
	view.CameraRight	= { viewMatrix.r[0].m128_f32[0], viewMatrix.r[1].m128_f32[0], viewMatrix.r[2].m128_f32[0] };
	view.CameraUp		= { viewMatrix.r[0].m128_f32[1], viewMatrix.r[1].m128_f32[1], viewMatrix.r[2].m128_f32[1] };

	view.StartSize	= m_Effect.StartSize;
	view.EndSize	= m_Effect.EndSize;
	view.StartColor	= m_Effect.StartColor;
	view.EndColor	= m_Effect.EndColor;

	return view;
}

// -------------------------------------------------------------------------------
// 下段 : パラメータ
//
// 変更があった行はDirtyを立て、未保存であることを上段に伝える
// -------------------------------------------------------------------------------
void Editor::EffectEditorPanel::DrawParameters(EditorContext& _ctx)
{
	EditorUI::Context&	ui		= *_ctx.pUI;
	EditorUI::Font&		font	= *_ctx.pFont;

	constexpr EditorUI::PropertyLayout kLayout{ 108.0f, 0.0f, 4.0f };

	bool changed = false;


	// -------------------------------------------------------------------------------
	// 発生
	// -------------------------------------------------------------------------------
	if (EditorUI::CollapsingHeader(ui, font, "Emitter"))
	{
		changed |= EditorUI::Property(ui, font, "Name", &m_Effect.Name, {}, kLayout);

		EditorUI::NumericEditorOptions<float> rateOptions;
		rateOptions.Min			= 0.0f;
		rateOptions.Max			= 20000.0f;
		rateOptions.DragSpeed	= 5.0L;
		rateOptions.Precision	= 0;
		rateOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Spawn Rate", &m_Effect.SpawnRate, rateOptions, kLayout);

		// 形状はボタンを横に並べて、いま選ばれているものを印で示す
		EditorUI::Text(ui, font, "Shape");

		for (int i = 0; i < static_cast<int>(std::size(kShapeLabels)); ++i)
		{
			if (i > 0)
			{
				EditorUI::SameLine(ui);
			}

			const bool selected = (static_cast<int>(m_Effect.Shape) == i);

			const std::string label = selected
				? (std::string("[") + std::string(kShapeLabels[i]) + "]")
				: std::string(kShapeLabels[i]);

			if (EditorUI::Button(ui, label, font, { 74.0f, 22.0f }))
			{
				m_Effect.Shape	= static_cast<EmitterShape>(i);
				changed			= true;
			}
		}

		EditorUI::NumericEditorOptions<float> radiusOptions;
		radiusOptions.Min		= 0.0f;
		radiusOptions.Max		= 20.0f;
		radiusOptions.DragSpeed	= 0.01L;
		radiusOptions.Precision	= 2;
		radiusOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Radius", &m_Effect.ShapeRadius, radiusOptions, kLayout);
	}

	// -------------------------------------------------------------------------------
	// 動き
	// -------------------------------------------------------------------------------
	if (EditorUI::CollapsingHeader(ui, font, "Motion"))
	{
		EditorUI::NumericEditorOptions<float> lifeOptions;
		lifeOptions.Min			= 0.05f;
		lifeOptions.Max			= 30.0f;
		lifeOptions.DragSpeed	= 0.01L;
		lifeOptions.Precision	= 2;
		lifeOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Life", &m_Effect.LifeTime, lifeOptions, kLayout);

		EditorUI::NumericEditorOptions<float> lifeRandomOptions = lifeOptions;
		lifeRandomOptions.Min = 0.0f;
		changed |= EditorUI::Property(ui, font, "Life Random", &m_Effect.LifeTimeRandom, lifeRandomOptions, kLayout);

		EditorUI::NumericEditorOptions<float> speedOptions;
		speedOptions.Min		= -100.0f;
		speedOptions.Max		=  100.0f;
		speedOptions.DragSpeed	= 0.05L;
		speedOptions.Precision	= 2;
		speedOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Speed", &m_Effect.InitialSpeed, speedOptions, kLayout);
		changed |= EditorUI::Property(ui, font, "Speed Random", &m_Effect.InitialSpeedRandom, speedOptions, kLayout);

		EditorUI::NumericEditorOptions<float> angleOptions;
		angleOptions.Min		= 0.0f;
		angleOptions.Max		= 360.0f;
		angleOptions.DragSpeed	= 1.0L;
		angleOptions.Precision	= 0;
		angleOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Spread", &m_Effect.SpreadAngle, angleOptions, kLayout);

		changed |= EditorUI::Property(ui, font, "Gravity", &m_Effect.Gravity, speedOptions, kLayout);

		EditorUI::NumericEditorOptions<float> dragOptions;
		dragOptions.Min			= 0.0f;
		dragOptions.Max			= 10.0f;
		dragOptions.DragSpeed	= 0.01L;
		dragOptions.Precision	= 2;
		dragOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Drag", &m_Effect.Drag, dragOptions, kLayout);
	}

	// -------------------------------------------------------------------------------
	// 見た目
	// -------------------------------------------------------------------------------
	if (EditorUI::CollapsingHeader(ui, font, "Appearance"))
	{
		EditorUI::NumericEditorOptions<float> sizeOptions;
		sizeOptions.Min			= 0.0f;
		sizeOptions.Max			= 10.0f;
		sizeOptions.DragSpeed	= 0.005L;
		sizeOptions.Precision	= 3;
		sizeOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Start Size", &m_Effect.StartSize, sizeOptions, kLayout);
		changed |= EditorUI::Property(ui, font, "End Size", &m_Effect.EndSize, sizeOptions, kLayout);

		// -------------------------------------------------------------------------------
		// 色は見本を押すと色相環のピッカーが開く
		// 数値だけでは仕上がりが想像しにくいため、視覚的に選べるようにしている
		// ピッカーの中にRGBAの数値欄もあるので、正確な値も指定できる
		// -------------------------------------------------------------------------------
		changed |= EditorUI::ColorProperty(ui, font, "Start Color", &m_Effect.StartColor, kLayout);
		changed |= EditorUI::ColorProperty(ui, font, "End Color", &m_Effect.EndColor, kLayout);
	}

	// -------------------------------------------------------------------------------
	// 再生
	// -------------------------------------------------------------------------------
	if (EditorUI::CollapsingHeader(ui, font, "Playback", false))
	{
		changed |= EditorUI::Property(ui, font, "Looping", &m_Effect.Looping, kLayout);

		EditorUI::NumericEditorOptions<float> durationOptions;
		durationOptions.Min			= 0.1f;
		durationOptions.Max			= 60.0f;
		durationOptions.DragSpeed	= 0.05L;
		durationOptions.Precision	= 2;
		durationOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Duration", &m_Effect.Duration, durationOptions, kLayout);

		// 最大数を変えるとプールを作り直すため、値は控えめに動かす
		EditorUI::NumericEditorOptions<int32_t> maxOptions;
		maxOptions.Min			= 256;
		maxOptions.Max			= 262144;
		maxOptions.DragSpeed	= 64.0L;
		changed |= EditorUI::Property(ui, font, "Max Particles", &m_Effect.MaxParticles, maxOptions, kLayout);
	}

	if (changed)
	{
		m_Dirty = true;
	}
}

// -------------------------------------------------------------------------------
// ファイルからの読み込み
// -------------------------------------------------------------------------------
bool Editor::EffectEditorPanel::LoadFromFile()
{
	if (m_Path.empty())
	{ return false; }

	std::ifstream file(m_Path, std::ios::binary);
	if (!file.is_open())
	{
		ELOG("EffectEditorPanel::LoadFromFile() failed to open file");
		return false;
	}

	const std::string contents(
		(std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());

	// 中身が空、または壊れている場合は既定値のまま開く
	// 新規作成直後のファイルでもエディタが開けるようにするため
	if (!DeserializeEffect(contents, m_Effect))
	{
		m_Effect.Name = m_Path.stem().string();
	}

	m_Dirty = false;
	return true;
}

// -------------------------------------------------------------------------------
// ファイルへの書き出し
// -------------------------------------------------------------------------------
bool Editor::EffectEditorPanel::SaveToFile()
{
	if (m_Path.empty())
	{ return false; }

	std::ofstream file(m_Path, std::ios::binary | std::ios::trunc);
	if (!file.is_open())
	{
		ELOG("EffectEditorPanel::SaveToFile() failed to open file");
		return false;
	}

	const std::string contents = SerializeEffect(m_Effect);
	file.write(contents.data(), static_cast<std::streamsize>(contents.size()));

	return true;
}
