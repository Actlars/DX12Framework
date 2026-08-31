#pragma once
// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include <Engine/EditorUI/Core/Context/Context.h>
#include <Engine/EditorUI/Text/TextLayout/TextLayout.h>

// -------------------------------------------------------------------------------
// Widgets
//
// 概要 :
//	EditorUIの基本ウィジェット群
//
//	即時モードのため、ウィジェットは状態を持たず、値は呼び出し元のものを
//	ポインタ経由で直接書き換える。「UIの状態」と「アプリの変数」を同期させる
//	コードが不要になり、常にアプリ側の変数が唯一の正になる
//
//	構成
//		Text 系		: 文字列の計測と描画
//		Value 系	: InputText / Drag* / Property。変数を直接編集する
//		Basic 系	: Button / Checkbox / Separator
//
//	戻り値の約束
//		bool を返すウィジェットは「このフレームで値が変わったか」を返す
//		（Buttonのみ「押されたか」）
// -------------------------------------------------------------------------------
namespace EditorUI
{
	class Font;

	// -------------------------------------------------------------------------------
	// Text API
	//
	//	TextOptions / TextMetrics / TextHorizontalAlignment は TextLayout.h で定義する
	//	計測(Measure)と描画(Text)がまったく同じ指定を受け取るため、
	//	「測った通りの大きさで描かれる」ことが保証される
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// @brief	テキストを描かずに、必要なサイズだけを求める
	//
	//	レイアウトを先に決めたい場合に使う。Contextを必要としないため、
	//	ウィンドウのBegin～Endの外からでも呼べる
	//
	// @param[in]	_font		計測に使うフォント
	// @param[in]	_utf8Text	計測する文字列(UTF-8)
	// @param[in]	_options	折り返しなどの指定
	// @return	計測結果
	// -------------------------------------------------------------------------------
	TextMetrics MeasureText(
		Font&				_font,
		std::string_view	_utf8Text,
		const TextOptions&	_options = {});

	TextMetrics MeasureText(
		Font&				_font,
		std::wstring_view	_text,
		const TextOptions&	_options = {});

	// -------------------------------------------------------------------------------
	// @brief	テキストを1つのウィジェットとして配置する
	//
	// @param[in]	_ctx		描画対象のContext
	// @param[in]	_font		描画に使うフォント
	// @param[in]	_utf8Text	描画する文字列(UTF-8)
	// @param[in]	_options	折り返し・行揃え・色などの指定
	// @return	実際に配置された結果のサイズ情報
	// -------------------------------------------------------------------------------
	TextMetrics Text(
		Context&			_ctx,
		Font&				_font,
		std::string_view	_utf8Text,
		const TextOptions&	_options = {});

	TextMetrics Text(
		Context&			_ctx,
		Font&				_font,
		std::wstring_view	_text,
		const TextOptions&	_options = {});

	// 色を指定してテキストを描く
	TextMetrics TextColored(
		Context&			_ctx,
		Font&				_font,
		std::string_view	_utf8Text,
		Color32				_color);

	// 折り返しを有効にしてテキストを描く。_wrapWidthが0ならコンテンツ幅で折り返す
	TextMetrics TextWrapped(
		Context&			_ctx,
		Font&				_font,
		std::string_view	_utf8Text,
		float				_wrapWidth = 0.0f);

	// 補足説明用の、控えめな色でテキストを描く
	TextMetrics TextMuted(
		Context&			_ctx,
		Font&				_font,
		std::string_view	_utf8Text);

	// -------------------------------------------------------------------------------
	// Editable value widgets
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// InputTextOptions struct
	//
	// 概要 :
	//	文字列入力欄の挙動の指定
	// -------------------------------------------------------------------------------
	struct InputTextOptions
	{
		// 入力欄のサイズ。0以下の成分は自動決定する
		//	Width  : コンテンツ領域いっぱい
		//	Height : フォントの行の高さ + 上下パディング
		DirectX::XMFLOAT2 Size{ 0.0f,0.0f };

		std::size_t MaxCharacters	= 4096;		// 入力できる最大文字数
		bool ReadOnly				= false;	// 表示のみで編集させない
		bool CommitOnFocusLoss		= true;		// 他をクリックしたときに確定するか(falseなら破棄)
		bool SelectAllOnFocus		= false;	// 編集開始時に全選択するか

		// -------------------------------------------------------------------------------
		// クリックを待たずに、その場で編集を始めるか
		//
		//	ヒエラルキーの名前変更のように、
		//	メニューを選んだ直後からすぐ打ち込めるようにするために使う
		//	内容は全選択された状態になり、そのまま打ち直せる
		//
		//	注意 : 「編集を始めたい最初の1フレームだけ」trueにする
		//	毎フレームtrueにすると、他をクリックして編集を終えても
		//	次のフレームでまた編集状態に戻ってしまう
		// -------------------------------------------------------------------------------
		bool ActivateNow			= false;
	};

	// -------------------------------------------------------------------------------
	// NumericEditorOptions struct
	//
	// 概要 :
	//	数値編集ウィジェット(Drag系)の指定
	//	扱う型ごとに範囲や刻み幅の型が変わるため、テンプレートにしている
	// -------------------------------------------------------------------------------
	template<typename T>
	struct NumericEditorOptions
	{
		T			Min			= (std::numeric_limits<T>::lowest)();
		T			Max			= (std::numeric_limits<T>::max)();
		T			Step		= static_cast<T>(1);	// キーボード入力時の丸め単位(0で無効)
		long double DragSpeed	= 0.1L;					// マウス1ピクセルあたりの変化量
		int			Precision	= 3;					// 小数の表示桁数(整数型では無視される)
		bool		Clamp		= true;					// Min/Maxで値を制限するか
		bool		ReadOnly	= false;
	};

	// -------------------------------------------------------------------------------
	// PropertyLayout struct
	//
	// 概要 :
	//	「ラベル + 入力欄」を1行に並べるプロパティ行のレイアウト指定
	//	同じ値を全プロパティで共有すると、ラベルの右端がそろって読みやすくなる
	// -------------------------------------------------------------------------------
	struct PropertyLayout
	{
		float LabelWidth		= 130.0f;	// ラベル部分の横幅
		float RowHeight			= 0.0f;		// 行の高さ。0以下ならフォントに合わせて自動
		float ComponentSpacing	= 4.0f;		// XYZWなど、複数成分どうしの間隔
	};

	// -------------------------------------------------------------------------------
	// @brief	文字列を編集する入力欄を配置する
	//
	//	クリックで編集を開始し、Enterまたは他所のクリックで確定、Escapeで破棄する
	//
	// @param[in]		_ctx		描画対象のContext
	// @param[in]		_idLabel	Id生成に使う識別名（画面には表示されない）
	// @param[in]		_font		描画に使うフォント
	// @param[in,out]	_value		編集対象の文字列(UTF-8)
	// @param[in]		_options	挙動の指定
	// @return	true : このフレームで値が確定した / false : それ以外
	// -------------------------------------------------------------------------------
	bool InputText(
		Context&				_ctx,
		std::string_view		_idLabel,
		Font&					_font,
		std::string*			_value,
		const InputTextOptions& _options = {});

	// -------------------------------------------------------------------------------
	// @brief	数値を左右のドラッグで編集する入力欄を配置する
	//
	//	ドラッグせずにクリックした場合はキーボード入力へ切り替わる
	//
	// @param[in]		_ctx		描画対象のContext
	// @param[in]		_idLabel	Id生成に使う識別名（画面には表示されない）
	// @param[in]		_font		描画に使うフォント
	// @param[in,out]	_value		編集対象の値
	// @param[in]		_options	範囲・刻み幅などの指定
	// @return	true : このフレームで値が変化した / false : それ以外
	// -------------------------------------------------------------------------------
	bool DragInt32(
		Context&								_ctx,
		std::string_view						_idLabel,
		Font&									_font,
		int32_t*								_value,
		const NumericEditorOptions<int32_t>&	_options = {});

	bool DragUInt32(
		Context&								_ctx,
		std::string_view						_idLabel,
		Font&									_font,
		uint32_t*								_value,
		const NumericEditorOptions<uint32_t>&	_options = {});

	bool DragFloat(
		Context&								_ctx,
		std::string_view						_idLabel,
		Font&									_font,
		float*									_value,
		const NumericEditorOptions<float>&		_options = {});

	bool DragDouble(
		Context&								_ctx,
		std::string_view						_idLabel,
		Font&									_font,
		double*									_value,
		const NumericEditorOptions<double>&		_options = {});

	// -------------------------------------------------------------------------------
	// @brief	「ラベル + 編集欄」を1行にまとめて配置する
	//
	//	インスペクタ上で変数を並べるための、いちばん外側の入り口
	//	型ごとにオーバーロードしているため、呼び出し側は常にProperty()と書けばよい
	//	XMFLOAT2/3/4は成分ごとに色分けされた複数の欄に展開される
	//
	// @param[in]		_ctx		描画対象のContext
	// @param[in]		_font		描画に使うフォント
	// @param[in]		_label		画面に表示するラベル。Idの生成にも使う
	// @param[in,out]	_value		編集対象の変数
	// @param[in]		_options	型ごとの編集指定
	// @param[in]		_layout		ラベル幅などの行レイアウト
	// @return	true : このフレームで値が変化した / false : それ以外
	// -------------------------------------------------------------------------------
	bool Property(
		Context&				_ctx,
		Font&					_font,
		std::string_view		_label,
		bool*					_value,
		const PropertyLayout&	_layout = {});

	bool Property(
		Context&				_ctx,
		Font&					_font,
		std::string_view		_label,
		std::string*			_value,
		const InputTextOptions& _options = {},
		const PropertyLayout&	_layout = {});

	bool Property(
		Context&								_ctx,
		Font&									_font,
		std::string_view						_label,
		int32_t*								_value,
		const NumericEditorOptions<int32_t>&	_options = {},
		const PropertyLayout&					_layout = {});

	bool Property(
		Context&								_ctx,
		Font&									_font,
		std::string_view						_label,
		uint32_t*								_value,
		const NumericEditorOptions<uint32_t>&	_options = {},
		const PropertyLayout&					_layout = {});

	bool Property(
		Context&							_ctx,
		Font&								_font,
		std::string_view					_label,
		float*								_value,
		const NumericEditorOptions<float>&	_options = {},
		const PropertyLayout&				_layout = {});

	bool Property(
		Context&							_ctx,
		Font&								_font,
		std::string_view					_label,
		double*								_value,
		const NumericEditorOptions<double>&	_options = {},
		const PropertyLayout&				_layout = {});

	bool Property(
		Context&							_ctx,
		Font&								_font,
		std::string_view					_label,
		DirectX::XMFLOAT2*					_value,
		const NumericEditorOptions<float>&	_options = {},
		const PropertyLayout&				_layout = {});

	bool Property(
		Context&							_ctx,
		Font&								_font,
		std::string_view					_label,
		DirectX::XMFLOAT3*					_value,
		const NumericEditorOptions<float>&	_options = {},
		const PropertyLayout&				_layout = {});

	bool Property(
		Context&							_ctx,
		Font&								_font,
		std::string_view					_label,
		DirectX::XMFLOAT4*					_value,
		const NumericEditorOptions<float>&	_options = {},
		const PropertyLayout&				_layout = {});

	// -------------------------------------------------------------------------------
	// Basic widgets
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// @brief	ボタンを配置する
	//
	// @param[in]	_ctx	描画対象のContext
	// @param[in]	_label	ボタンのラベル(表示とIdの生成を兼ねる)
	// @param[in]	_font	描画に使うフォント
	// @param[in]	_size	ボタンの最小サイズ。ラベルが収まらなければ自動で広がる
	// @return	true : このフレームでクリックが成立した / false : それ以外
	// -------------------------------------------------------------------------------
	bool Button(Context& _ctx, std::string_view _label, Font& _font, DirectX::XMFLOAT2 _size = { 120.0f,24.0f });

	// -------------------------------------------------------------------------------
	// @brief	チェックボックスを配置する
	//
	// 即時モード方式のため、_pValueが指す値をこの関数の中で直接書き換える
	//
	// @param[in]		_ctx		描画対象のContext
	// @param[in]		_label		チェックボックスのラベル(IDの生成に使う)
	// @param[in,out]	_pValue		チェック状態を持つ変数へのポインタ
	// @param[in]		_boxSize	チェックボックス本体の一辺のサイズ
	// @return	true : このフレームでクリックされ、値がトグルされた / false : それ以外
	// -------------------------------------------------------------------------------
	bool Checkbox(Context& _ctx, std::string_view _label, bool* _pValue, float _boxSize = 16.0f);

	// -------------------------------------------------------------------------------
	// @brief	水平の区切り線を1本引き、1行分のカーソルを進める
	//
	// @param[in]	_ctx	描画対象のContext
	// -------------------------------------------------------------------------------
	void Separator(Context& _ctx);

	// -------------------------------------------------------------------------------
	// @brief	直前のウィジェットと同じ行に、次のウィジェットを続けて配置する
	//
	// @param[in]	_ctx		描画対象のContext
	// @param[in]	_spacing	直前のウィジェットとの間隔。0以下ならスタイルの既定値
	// -------------------------------------------------------------------------------
	void SameLine(Context& _ctx, float _spacing = 0.0f);

	// -------------------------------------------------------------------------------
	// @brief	次に配置するウィジェット1つの横幅を指定する
	//
	// @param[in]	_ctx	描画対象のContext
	// @param[in]	_width	横幅。0以下でコンテンツ領域いっぱい
	// -------------------------------------------------------------------------------
	void SetNextItemWidth(Context& _ctx, float _width);

	// -------------------------------------------------------------------------------
	// @brief	以降の行を1段深くインデントする。TreeNodeの中身を字下げするのに使う
	// -------------------------------------------------------------------------------
	void Indent(Context& _ctx, float _width = 0.0f);
	void Unindent(Context& _ctx, float _width = 0.0f);

	// -------------------------------------------------------------------------------
	// Container / List widgets
	//
	//	ヒエラルキーやコンテンツブラウザのように「並んだ行を選ぶ」UIのための部品
	//	行1つを表す矩形と、その中でのクリック結果を共通の形で返す
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// ItemInteraction struct
	//
	// 概要 :
	//	1行ぶんのウィジェットに対する、このフレームの操作結果
	//	選択の更新・右クリックメニュー・ダブルクリックで開く、をすべてここから判断する
	// -------------------------------------------------------------------------------
	struct ItemInteraction
	{
		bool Hovered		= false;
		bool Clicked		= false;	// 左クリックで選択された
		bool RightClicked	= false;	// 右クリックされた（コンテキストメニュー）
		bool DoubleClicked	= false;	// 左ダブルクリックされた（開く）
	};

	// -------------------------------------------------------------------------------
	// @brief	1行ぶんの選択可能な行を配置する
	//
	// @param[in]	_ctx		描画対象のContext
	// @param[in]	_font		描画に使うフォント
	// @param[in]	_label		表示する文字列(Idの生成も兼ねる)
	// @param[in]	_selected	選択状態として描くか
	// @param[in]	_height		行の高さ。0以下ならフォントに合わせて自動
	// @return	このフレームの操作結果
	// -------------------------------------------------------------------------------
	ItemInteraction Selectable(
		Context&			_ctx,
		Font&				_font,
		std::string_view	_label,
		bool				_selected,
		float				_height = 0.0f);

	// -------------------------------------------------------------------------------
	// TreeNodeOptions struct
	//
	// 概要 :
	//	ツリー1ノードの見た目と初期状態の指定
	// -------------------------------------------------------------------------------
	struct TreeNodeOptions
	{
		bool IsLeaf		= false;	// 子を持たない。開閉の三角形を描かない
		bool Selected	= false;	// 選択状態として描くか
		bool DefaultOpen = false;	// 初回表示時に開いた状態にするか
	};

	// -------------------------------------------------------------------------------
	// TreeNodeResult struct
	//
	// 概要 :
	//	ツリー1ノードの結果
	//	Openは「子を描くべきか」を表し、trueならTreePopと対で使う
	// -------------------------------------------------------------------------------
	struct TreeNodeResult
	{
		bool			Open = false;	// trueなら子を描き、必ずTreePopを呼ぶ
		ItemInteraction	Interaction{};	// 行そのものへの操作結果
	};

	// -------------------------------------------------------------------------------
	// @brief	開閉できるツリーの1ノードを配置する
	//
	//	開閉状態はContextのストレージがIdをキーに保持するため、
	//	呼び出し側が開閉フラグを持つ必要はない
	//
	// @param[in]	_ctx		描画対象のContext
	// @param[in]	_font		描画に使うフォント
	// @param[in]	_label		表示する文字列(Idの生成も兼ねる)
	// @param[in]	_options	葉かどうか・選択状態・初期開閉
	// @return	Openがtrueなら子を描き、TreePopで閉じる
	// -------------------------------------------------------------------------------
	TreeNodeResult TreeNode(
		Context&				_ctx,
		Font&					_font,
		std::string_view		_label,
		const TreeNodeOptions&	_options = {});

	// @brief	TreeNodeがOpenを返したときに、子を描き終えてから呼ぶ
	void TreePop(Context& _ctx);

	// -------------------------------------------------------------------------------
	// @brief	折り畳める見出しを配置する。戻り値がtrueなら中身を描く
	// -------------------------------------------------------------------------------
	bool CollapsingHeader(
		Context&			_ctx,
		Font&				_font,
		std::string_view	_label,
		bool				_defaultOpen = true);

	// -------------------------------------------------------------------------------
	// @brief	見出し付きの区切り線を引く
	// -------------------------------------------------------------------------------
	void SeparatorText(Context& _ctx, Font& _font, std::string_view _label);

	// -------------------------------------------------------------------------------
	// @brief	指定サイズの空白を1行として確保する
	//
	//	レイアウトの調整や、後から矩形を得たい場合の場所取りに使う
	// @return	確保された矩形(スクリーン座標)
	// -------------------------------------------------------------------------------
	Rect2D Dummy(Context& _ctx, const DirectX::XMFLOAT2& _size);

	// -------------------------------------------------------------------------------
	// Image widgets
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// @brief	テクスチャを1つのウィジェットとして配置する
	//
	//	ゲーム画面のレンダーターゲットを、そのままウィンドウの中身として表示するために使う
	//
	// @param[in]	_ctx		描画対象のContext
	// @param[in]	_texture	Rendererに登録済みのテクスチャId
	// @param[in]	_size		表示サイズ
	// @param[in]	_uv			サンプリングするUV範囲
	// @return	配置された矩形(スクリーン座標)。マウス判定に使える
	// -------------------------------------------------------------------------------
	Rect2D Image(
		Context&					_ctx,
		TextureId					_texture,
		const DirectX::XMFLOAT2&	_size,
		const Rect2D&				_uv = Rect2D{ {0.0f,0.0f},{1.0f,1.0f} });

	// -------------------------------------------------------------------------------
	// Popup / Context menu
	//
	//	右クリックで開くメニューを、次の3つの部品だけで組み立てられるようにする
	//		BeginPopupContextWindow / BeginPopupContextItem  … 開く条件
	//		MenuItem                                        … 項目
	//		EndPopup                                        … 閉じる
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// @brief	現在のウィンドウの何もない場所を右クリックしたときに開くメニューを開始する
	//
	//	trueが返った場合のみ中身を積み、必ずEndPopupを呼ぶ
	// -------------------------------------------------------------------------------
	bool BeginPopupContextWindow(Context& _ctx, std::string_view _idLabel);

	// -------------------------------------------------------------------------------
	// @brief	直前に配置したウィジェットを右クリックしたときに開くメニューを開始する
	//
	//	Selectable / TreeNode の直後に呼ぶことで、その行のメニューになる
	// -------------------------------------------------------------------------------
	bool BeginPopupContextItem(Context& _ctx, std::string_view _idLabel, bool _rightClicked);

	// @brief	名前を指定してメニューを開く。ボタンから開く場合などに使う
	void OpenPopup(Context& _ctx, std::string_view _idLabel);

	// @brief	開いていればメニューを開始する
	bool BeginPopup(Context& _ctx, std::string_view _idLabel);
	void EndPopup(Context& _ctx);

	// -------------------------------------------------------------------------------
	// @brief	メニュー項目を1つ配置する。選ばれたらメニューを閉じてtrueを返す
	//
	// @param[in]	_ctx		描画対象のContext
	// @param[in]	_font		描画に使うフォント
	// @param[in]	_label		項目名
	// @param[in]	_enabled	falseなら灰色で表示し、選べないようにする
	// -------------------------------------------------------------------------------
	bool MenuItem(
		Context&			_ctx,
		Font&				_font,
		std::string_view	_label,
		bool				_enabled = true);

	// @brief	メニュー内の区切り線
	void MenuSeparator(Context& _ctx);

	// -------------------------------------------------------------------------------
	// Color widgets
	//
	//	色は数値だけでは決めにくいため、円（色相環）から直感的に選べるようにする
	//	扱う値は常にRGBA(0.0～1.0)で、HSVへの変換はウィジェットの中だけで完結する
	// -------------------------------------------------------------------------------

	// -------------------------------------------------------------------------------
	// @brief	現在の色を示す四角を置く
	//
	//	半透明でも分かるよう、下地に市松模様を敷く
	//
	// @param[in]	_ctx		描画対象のContext
	// @param[in]	_idLabel	Id生成に使う識別名（画面には表示されない）
	// @param[in]	_color		表示する色(RGBA 0.0～1.0)
	// @param[in]	_size		見本の大きさ。0以下の成分は自動決定する
	// @return	true : このフレームでクリックされた
	// -------------------------------------------------------------------------------
	bool ColorButton(
		Context&					_ctx,
		std::string_view			_idLabel,
		const DirectX::XMFLOAT4&	_color,
		const DirectX::XMFLOAT2&	_size = { 0.0f, 0.0f });

	// -------------------------------------------------------------------------------
	// @brief	色相環によるカラーピッカーを配置する
	//
	//	角度で色相、中心からの距離で彩度を選ぶ
	//	右側に明度と不透明度のスライダ、下に数値欄を並べる
	//
	// @param[in]		_ctx		描画対象のContext
	// @param[in]		_font		描画に使うフォント
	// @param[in]		_idLabel	Id生成に使う識別名
	// @param[in,out]	_color		編集対象の色(RGBA 0.0～1.0)
	// @return	true : このフレームで色が変化した
	// -------------------------------------------------------------------------------
	bool ColorPicker4(
		Context&			_ctx,
		Font&				_font,
		std::string_view	_idLabel,
		DirectX::XMFLOAT4*	_color);

	// -------------------------------------------------------------------------------
	// @brief	「ラベル + 色見本」を1行に置く。見本を押すとピッカーが開く
	//
	//	インスペクタの行を1行に保ちつつ、必要なときだけ大きなピッカーを出せる
	// -------------------------------------------------------------------------------
	bool ColorProperty(
		Context&				_ctx,
		Font&					_font,
		std::string_view		_label,
		DirectX::XMFLOAT4*		_color,
		const PropertyLayout&	_layout = {});

}
