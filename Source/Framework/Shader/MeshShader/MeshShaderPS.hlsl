#include "MeshShaderInc.hlsli"

PSOutput main(VSOutput input) : SV_TARGET
{
    PSOutput output;
    float4 texColor = g_Texture.Sample(g_Sampler, input.TexCoord);
    
    output.Color = texColor /* * input.Color*/;
    return output;
}
