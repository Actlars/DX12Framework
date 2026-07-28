#pragma once

namespace EditorUI
{
	// -------------------------------------------------------------------------------
	// Rect2D struct
	// 
	// 概要 : 
	//	左上（Min）と右下（Max）で矩形を表す
	// -------------------------------------------------------------------------------
	using Color32 = uint32_t;

	struct Rect2D
	{
		DirectX::XMFLOAT2 Min{ 0.0f,0.0f };	// 左上(スクリーン座標)
		DirectX::XMFLOAT2 Max{ 0.0f,0.0f };	// 右下(スクリーン座標)

		// 差分から計算する派生値
		float Width()	const { return Max.x - Min.x; }
		float Height()	const { return Max.y - Min.y; }

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
