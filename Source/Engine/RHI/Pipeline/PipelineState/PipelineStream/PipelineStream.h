#pragma once

namespace RHI
{
	// -------------------------------------------------------------------------------
	// StateParam structure
	// 
	// 概要 : 
	//	D3D12_PIPELINE_STATE_SUBOBJECT_TYPEと値をペアにして保存するテンプレート
	//	PipelineStateStream（可変長サブオブジェクト列）に積むための最小単位
	// -------------------------------------------------------------------------------
	template<typename ValueType, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE ObjectType>
	class alignas(void*)StateParam
	{
	public:

		StateParam()
		:Type(ObjectType)
		,Value(ValueType())
		{ /* DO_NOTHING */ }

		StateParam(const ValueType& _value)
		:Value(_value)
		,Type(ObjectType)
		{ /* DO_NOTHING */ }

		StateParam& operator = (const ValueType& _value)
		{
			Type = ObjectType;
			Value = _value;
			return *this;
		}

	private:

		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type;
		ValueType							Value;
	};

	// 長いから省略形を作る
	#define PSST(x) D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_##x
	using SP_ROOT_SIGNATURE = StateParam<ID3D12RootSignature*,			PSST(ROOT_SIGNATURE)>;
	using SP_AS				= StateParam<D3D12_SHADER_BYTECODE,			PSST(AS)>;
	using SP_MS				= StateParam<D3D12_SHADER_BYTECODE,			PSST(MS)>;
	using SP_PS				= StateParam<D3D12_SHADER_BYTECODE,			PSST(PS)>;
	using SP_BLEND			= StateParam<D3D12_BLEND_DESC,				PSST(BLEND)>;
	using SP_RASTERIZER		= StateParam<D3D12_RASTERIZER_DESC,			PSST(RASTERIZER)>;
	using SP_DEPTH_STENCIL	= StateParam<D3D12_DEPTH_STENCIL_DESC,		PSST(DEPTH_STENCIL)>;
	using SP_SAMPLE_MASK	= StateParam<UINT,							PSST(SAMPLE_MASK)>;
	using SP_SAMPLE_DESC	= StateParam<DXGI_SAMPLE_DESC,				PSST(SAMPLE_DESC)>;
	using SP_RT_FORMAT		= StateParam<D3D12_RT_FORMAT_ARRAY,			PSST(RENDER_TARGET_FORMATS)>;
	using SP_DS_FORMAT		= StateParam<DXGI_FORMAT,					PSST(DEPTH_STENCIL_FORMAT)>;
	using SP_FLAGS			= StateParam<D3D12_PIPELINE_STATE_FLAGS,	PSST(FLAGS)>;
	// 宣言し終わったら要らないので無効化.
	#undef PSST

	// -------------------------------------------------------------------------------
	// MeshShaderPipelineStateDesc
	// 
	// 概要 : 
	//	MeshShaderパイプライン用のPipelineStream本体
	// -------------------------------------------------------------------------------
	struct MeshShaderPipelineStateDesc
	{
		SP_ROOT_SIGNATURE  RootSignature;           // ルートシグニチャ
		SP_AS              AS;                      // 増幅シェーダ
		SP_MS              MS;                      // メッシュシェーダ
		SP_PS              PS;                      // ピクセルシェーダ
		SP_BLEND           Blend;                   // ブレンドステート
		SP_RASTERIZER      Rasterizer;              // ラスタライザーステート
		SP_DEPTH_STENCIL   DepthStencil;            // 深度ステンシルステート
		SP_SAMPLE_MASK     SampleMask;              // サンプルマスク
		SP_SAMPLE_DESC     SampleDesc;              // サンプル設定
		SP_RT_FORMAT       RTFormats;               // レンダーゲットフォーマット
		SP_DS_FORMAT       DSFormat;                // 深度ステンシルフォーマット
		SP_FLAGS           Flags;                   // フラグ
	};
}
