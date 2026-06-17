// -------------------------------------------------------------------------------
// Includes
// -------------------------------------------------------------------------------
#include "SimpleInc.hlsli"

PSOutput main(VSOutput input)
{
    PSOutput output = (PSOutput) 0;
	
    output.Color = input.Color;
	
    return output;
}
