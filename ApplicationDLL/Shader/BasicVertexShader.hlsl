#include "BasicShaderHeader.hlsli"

//•ÏŠ·‚ğ‚Ü‚Æ‚ß‚½\‘¢‘Ì
cbuffer cbuff0 : register(b0)
{
    matrix mat; //•ÏŠ·s—ñ
};

Output BasicVS( float4 pos : POSITION, float2 uv : TEXCOORD )
{
	Output output;
    output.svpos = mul(mat, pos);
    output.uv = uv;
	return output;
}
