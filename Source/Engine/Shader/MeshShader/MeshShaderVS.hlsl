#include "MeshShaderInc.hlsli"

VSOutput main( VSInput _input )
{
    VSOutput output = (VSOutput) 0;
    
    float4 localPos = float4(_input.Position, 1.0f);
    float4 worldPos = mul(World, localPos);
    float4 viewPos = mul(View, worldPos);
    float4 projPos = mul(Proj, viewPos);
    
    output.Position = projPos;
    output.TexCoord = _input.TexCoord;
    output.WorldPos = worldPos;
    output.Normal   = normalize(mul((float3x3) World, _input.Normal));
    
    return output;
}
