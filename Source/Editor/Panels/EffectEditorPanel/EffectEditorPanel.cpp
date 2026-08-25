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
	// ウィンドウのタイトルはIdの元になるため、必ず一意にする
	// 「Effect: <ファイル名>」という形にすることで、
	// 同じファイルを二度開こうとしたときにFindで拾える
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

	// エミッタ形状の選択肢。ラジオボタン代わりにボタンを並べる
	constexpr std::string_view kShapeLabels[] = { "Point", "Circle", "Box" };
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
	// エフェクトの編集にはプレビューの縦幅とパラメータの両方が要るため、
	// 最初から縦に長めのウィンドウで開く
	SetInitialPlacement({ 200.0f, 120.0f }, { 380.0f, 520.0f });

	if (!m_Path.empty())
	{
		m_Effect.Name = m_Path.stem().string();
	}
}

// -------------------------------------------------------------------------------
// エフェクトエディタの中身
// -------------------------------------------------------------------------------
void Editor::EffectEditorPanel::OnGUI(EditorContext& _ctx)
{
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

	if (EditorUI::Button(ui, m_Preview.IsPlaying() ? "Pause" : "Play", font, { 64.0f, 24.0f }))
	{
		m_Preview.IsPlaying() ? m_Preview.Pause() : m_Preview.Play();
	}

	EditorUI::SameLine(ui);

	if (EditorUI::Button(ui, "Restart", font, { 72.0f, 24.0f }))
	{
		m_Preview.Restart();
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

	// 状態表示。未保存かどうかと、いま何粒生きているかが一目で分かるようにする
	const std::string status =
		std::string(m_Dirty ? "* 未保存   " : "保存済み   ") +
		"Particles: " + std::to_string(m_Preview.GetLiveCount());

	EditorUI::TextMuted(ui, font, status);
}

// -------------------------------------------------------------------------------
// 中段 : プレビュー
//
// 場所取り(Dummy)で領域を確保してから、その矩形の中へ直接粒を描く
// レイアウトの流れに乗せることで、上下のウィジェットと自然に並ぶ
// -------------------------------------------------------------------------------
void Editor::EffectEditorPanel::DrawPreview(EditorContext& _ctx)
{
	EditorUI::Context&		ui		= *_ctx.pUI;
	EditorUI::WindowFrame*	pFrame	= ui.GetCurrentWindow();

	if (pFrame == nullptr)
	{ return; }

	const EditorUI::Style& style = ui.GetStyle();

	const float width = EditorUI::GetContentWidth(pFrame, style);
	const EditorUI::Rect2D bounds = EditorUI::Dummy(ui, { width, m_PreviewHeight });

	if (!bounds.IsValid())
	{ return; }

	// プレビューの背景。ここが「舞台」であることを分かるようにする
	pFrame->Draw.AddRectFilled(bounds, style.ColorPanelBg);
	pFrame->Draw.AddRectOutline(bounds, style.ColorBorderLight, style.BorderThickness);

	// -------------------------------------------------------------------------------
	// 発生位置は領域の下寄り中央
	// 上に向かって噴き上がる形が、いちばん動きを確認しやすい
	// -------------------------------------------------------------------------------
	const DirectX::XMFLOAT2 origin
	{
		bounds.Min.x + bounds.Width() * 0.5f,
		bounds.Min.y + bounds.Height() * 0.75f
	};

	m_Preview.Update(m_Effect, _ctx.DeltaTime, origin);
	m_Preview.Draw(m_Effect, pFrame->Draw, bounds);
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

	constexpr EditorUI::PropertyLayout kLayout{ 104.0f, 0.0f, 4.0f };

	bool changed = false;

	// -------------------------------------------------------------------------------
	// 発生
	// -------------------------------------------------------------------------------
	if (EditorUI::CollapsingHeader(ui, font, "Emitter"))
	{
		changed |= EditorUI::Property(ui, font, "Name", &m_Effect.Name, {}, kLayout);

		EditorUI::NumericEditorOptions<float> rateOptions;
		rateOptions.Min			= 0.0f;
		rateOptions.Max			= 2000.0f;
		rateOptions.DragSpeed	= 1.0L;
		rateOptions.Precision	= 0;
		rateOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Spawn Rate", &m_Effect.SpawnRate, rateOptions, kLayout);

		// 形状はボタンを横に並べて、いま選ばれているものを色で示す
		EditorUI::Text(ui, font, "Shape");

		for (int i = 0; i < static_cast<int>(std::size(kShapeLabels)); ++i)
		{
			if (i > 0)
			{
				EditorUI::SameLine(ui);
			}

			const bool selected = (static_cast<int>(m_Effect.Shape) == i);

			// 選択中は先頭に印を付けて、ラベルだけで状態が分かるようにする
			const std::string label =
				(selected ? std::string("[") + std::string(kShapeLabels[i]) + "]"
						  : std::string(kShapeLabels[i]));

			if (EditorUI::Button(ui, label, font, { 72.0f, 22.0f }))
			{
				m_Effect.Shape	= static_cast<EmitterShape>(i);
				changed			= true;
			}
		}

		EditorUI::NumericEditorOptions<float> radiusOptions;
		radiusOptions.Min		= 0.0f;
		radiusOptions.Max		= 400.0f;
		radiusOptions.DragSpeed	= 0.5L;
		radiusOptions.Precision	= 1;
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
		lifeOptions.Max			= 20.0f;
		lifeOptions.DragSpeed	= 0.01L;
		lifeOptions.Precision	= 2;
		lifeOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Life", &m_Effect.LifeTime, lifeOptions, kLayout);

		EditorUI::NumericEditorOptions<float> lifeRandomOptions = lifeOptions;
		lifeRandomOptions.Min = 0.0f;
		changed |= EditorUI::Property(ui, font, "Life Random", &m_Effect.LifeTimeRandom, lifeRandomOptions, kLayout);

		EditorUI::NumericEditorOptions<float> speedOptions;
		speedOptions.Min		= -2000.0f;
		speedOptions.Max		=  2000.0f;
		speedOptions.DragSpeed	= 1.0L;
		speedOptions.Precision	= 1;
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
		sizeOptions.Max			= 200.0f;
		sizeOptions.DragSpeed	= 0.1L;
		sizeOptions.Precision	= 1;
		sizeOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Start Size", &m_Effect.StartSize, sizeOptions, kLayout);
		changed |= EditorUI::Property(ui, font, "End Size", &m_Effect.EndSize, sizeOptions, kLayout);

		// 色は0.0～1.0のRGBAとして編集する
		EditorUI::NumericEditorOptions<float> colorOptions;
		colorOptions.Min		= 0.0f;
		colorOptions.Max		= 1.0f;
		colorOptions.DragSpeed	= 0.005L;
		colorOptions.Precision	= 2;
		colorOptions.Step		= 0.0f;
		changed |= EditorUI::Property(ui, font, "Start Color", &m_Effect.StartColor, colorOptions, kLayout);
		changed |= EditorUI::Property(ui, font, "End Color", &m_Effect.EndColor, colorOptions, kLayout);
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

		EditorUI::NumericEditorOptions<int32_t> maxOptions;
		maxOptions.Min			= 1;
		maxOptions.Max			= 4096;
		maxOptions.DragSpeed	= 2.0L;
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
