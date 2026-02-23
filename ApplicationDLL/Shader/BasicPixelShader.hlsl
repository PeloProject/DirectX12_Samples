#include "BasicShaderHeader.hlsli"

// 0番スロットのテクスチャをサンプリングするための宣言
Texture2D<float4> g_texture0 : register(t0);
SamplerState g_sampler0 : register(s0);


float4 BasicPS(Output input) : SV_TARGET
{
    //float4 color = float4(input.svpos.x * 0.5f, (1.0f + input.svpos.y) * 0.5f, 1.0f, 1.0f);
    //float4 color2 = float4((float2(0, 1) + input.svpos.xy) * 0.5f, 1, 1);
    //return float4(input.uv, 1, 1);
    return float4(g_texture0.Sample(g_sampler0, input.uv));
	//return float4(1,1, 1, 1);
}