// -------------------------------------------------------------------------------
// ParticlePS.slang
//
// 概要 :
//  四角形を、中心が明るく端へ向かって消える円形の粒として塗る
//
//  テクスチャを使わずに済ませているのは、
//  アセットの用意なしにエフェクトの動きを確認できるようにするため
//  必要になればここをテクスチャのサンプリングへ差し替えればよい
// -------------------------------------------------------------------------------

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
    float4 Color    : COLOR0;
};

[shader("fragment")]
float4 main(PSInput _input) : SV_TARGET
{
    // 中心からの距離(0.0 = 中心, 1.0 = 外周)
    const float2 centered = _input.UV * 2.0f - 1.0f;
    const float  distance = length(centered);

    // 外周をなめらかに消す
    // 四角い縁が見えないよう、1.0を少し超えたところで完全に0になるようにする
    const float falloff = saturate(1.0f - distance);

    // 2乗にすると中心が締まって、粒らしい見え方になる
    const float alpha = falloff * falloff;

    if (alpha <= 0.001f)
    {
        discard;    // 完全に透明な部分は書き込まない
    }

    return float4(_input.Color.rgb, _input.Color.a * alpha);
}
