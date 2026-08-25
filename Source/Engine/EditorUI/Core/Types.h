#pragma once

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// Color32
	//
	// 概要 :
	//	頂点カラー1つ分の32bit値
	//
	//	頂点バッファのフォーマットは DXGI_FORMAT_R8G8B8A8_UNORM なので、
	//	GPUはこの4バイトを「メモリの先頭から R, G, B, A」の順で読む
	//	x86はリトルエンディアンのため、uint32_tの最下位バイトが先頭に置かれる
	//	つまりリテラルとして書いたときの並びは 0xAABBGGRR になる
	//
	//	16進で直接書くと必ずここで取り違えるため、色の指定は必ず MakeColor を使う
	// -------------------------------------------------------------------------------
	using Color32 = uint32_t;

	// 白テクスチャ用
	using		TextureId = uint64_t;
	constexpr	TextureId kWhiteTexture = 0;

	// -------------------------------------------------------------------------------
	// @brief	R,G,B,A(各0～255)から頂点カラーを組み立てる
	//
	//	並び替えをこの1か所に閉じ込めることで、呼び出し側は常に「赤,緑,青,不透明度」の
	//	順で書けばよくなる
	// -------------------------------------------------------------------------------
	constexpr Color32 MakeColor(uint32_t _r, uint32_t _g, uint32_t _b, uint32_t _a = 255u)
	{
		return (_a << 24) | (_b << 16) | (_g << 8) | _r;
	}

	// -------------------------------------------------------------------------------
	// @brief	色はそのままに、不透明度だけを差し替える
	//
	//	ホバー時の薄い塗りや、ドッキングのプレビューのように
	//	「同じ色を半透明で重ねたい」場面で使う
	// -------------------------------------------------------------------------------
	constexpr Color32 WithAlpha(Color32 _color, uint32_t _alpha)
	{
		return (_color & 0x00FFFFFFu) | (_alpha << 24);
	}

	// -------------------------------------------------------------------------------
	// @brief	2色を_tの割合で混ぜる(0.0で_a、1.0で_b)
	//
	//	スタイルに中間色を1つずつ足していくとキリがないため、
	//	「少し明るく」「少し暗く」を必要な場所でその場で作れるようにする
	// -------------------------------------------------------------------------------
	inline Color32 LerpColor(Color32 _a, Color32 _b, float _t)
	{
		const float t = std::clamp(_t, 0.0f, 1.0f);

		// 4バイトそれぞれを独立に補間する。並びはMakeColorと同じ(R,G,B,A)
		uint32_t result = 0;
		for (int i = 0; i < 4; ++i)
		{
			const int shift = i * 8;

			const float lhs = static_cast<float>((_a >> shift) & 0xFFu);
			const float rhs = static_cast<float>((_b >> shift) & 0xFFu);

			const uint32_t mixed = static_cast<uint32_t>(lhs + (rhs - lhs) * t + 0.5f);
			result |= (std::min)(mixed, 255u) << shift;
		}
		return result;
	}

	// -------------------------------------------------------------------------------
	// Rect2D struct
	//
	// 概要 :
	//	左上（Min）と右下（Max）で矩形を表す
	// -------------------------------------------------------------------------------
	struct Rect2D
	{
		DirectX::XMFLOAT2 Min{ 0.0f,0.0f };	// 左上(スクリーン座標)
		DirectX::XMFLOAT2 Max{ 0.0f,0.0f };	// 右下(スクリーン座標)

		// 差分から計算する派生値
		float Width()	const { return Max.x - Min.x; }
		float Height()	const { return Max.y - Min.y; }

		// 中心座標。ドロップ位置の判定やアイコンの中央寄せに使う
		DirectX::XMFLOAT2 Center() const
		{
			return { (Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f };
		}

		// 面積を持つ矩形か。クリップの結果が潰れたかどうかの判定に使う
		bool IsValid() const { return Max.x > Min.x&& Max.y > Min.y; }

		// マウス座標がこの矩形の内側にあるかを判定する
		bool Contains(const DirectX::XMFLOAT2& _point) const
		{
			return _point.x >= Min.x && _point.x <= Max.x && _point.y >= Min.y && _point.y <= Max.y;
		}

		// 元の矩形を上下左右に_amoutだけ膨らませた矩形を返す
		Rect2D Expanded(float _amount) const
		{
			return { {Min.x - _amount, Min.y - _amount}, {Max.x + _amount, Max.y + _amount} };
		}

		// 呼び出し時にRect2D::Equals(a,b)という形で呼び出すことで、対称な関係を表す
		// staticをつけず、a.Equals(b)でも呼び出せるように書けるが、主と従が分かれているわけではないため、staticを用いて対象に表せるようにしている
		// 関数内で引数の要素しか扱わないという判断でいい。

		// 2つの矩形が完全に一致しているかを返す
		static bool Equals(const Rect2D& _a, const Rect2D& _b)
		{
			return	_a.Min.x == _b.Min.x && _a.Min.y == _b.Min.y &&
					_a.Max.x == _b.Max.x && _a.Max.y == _b.Max.y;
		}

		// -------------------------------------------------------------------------------
		// @brief	2つの矩形の重なり(積集合)を返す
		//
		//	重なりがない場合はMinとMaxが逆転した「面積0以下」の矩形になる
		//	IsValid()がfalseになるので、呼び出し側はそれで空を判定できる
		// -------------------------------------------------------------------------------
		static Rect2D Intersect(const Rect2D& _a, const Rect2D& _b)
		{
			return Rect2D
			{
				{ (std::max)(_a.Min.x, _b.Min.x), (std::max)(_a.Min.y, _b.Min.y) },
				{ (std::min)(_a.Max.x, _b.Max.x), (std::min)(_a.Max.y, _b.Max.y) }
			};
		}
	};

	// クラスor構造体外にある関数なのでinlineをつけている。

	// 「左上座標 + サイズ」という指定方法からRect2Dを組み立てる
	inline Rect2D MakeRect(const DirectX::XMFLOAT2& _pos, const DirectX::XMFLOAT2& _size)
	{
		return Rect2D{ _pos,{_pos.x + _size.x, _pos.y + _size.y} };
	}

	// クリップなしを表す番兵値。クリップスタックの初期値として使う
	inline Rect2D MakeInfiniteRect()
	{
		return Rect2D{ {-FLT_MAX, -FLT_MAX}, {FLT_MAX, FLT_MAX} };
	}
}
