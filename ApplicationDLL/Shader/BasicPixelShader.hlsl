#pragma enable_d3d11_debug_symbols

struct Input
{
	float4 pos:POSITION;
	float4 svpos:SV_POSITION;
};

float4 BasicPS(Input input) : SV_TARGET
{
	float4 color = float4(input.pos.x *0.5f, (1.0f+ input.pos.y)*0.5f, 1.0f, 1.0f);
	float4 color2 = float4((float2(0,1)+ input.pos.xy)*0.5f,1,1);
	return color;
}