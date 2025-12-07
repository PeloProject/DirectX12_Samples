#include "BasicShaderHeader.hlsli"


float4 BasicPS(Output input) : SV_TARGET
{
	//float4 color = float4(input.pos.x *0.5f, (1.0f+ input.pos.y)*0.5f, 1.0f, 1.0f);
	//float4 color2 = float4((float2(0,1)+ input.pos.xy)*0.5f,1,1);
    return float4(input.uv, 1, 1);
}