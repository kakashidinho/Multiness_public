Texture2D nestex : register (t0);
SamplerState nestex_sampler : register (s0);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	return nestex.Sample(nestex_sampler, input.texcoord).zyxw;
}