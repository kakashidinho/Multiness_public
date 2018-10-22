cbuffer TransformConstantBuffer : register(b0)
{
	float4 transform;
};

cbuffer OrientationConstantBuffer : register(b1)
{
	float4x4 orientation;
};

// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 texcoord : TEXCOORD0;
};

PixelShaderInput main(float4 pos_texcoord : POSITION_TEXCOORD)
{
	PixelShaderInput re;

	float2 position = pos_texcoord.xy;
	float2 texcoord = pos_texcoord.zw;

	float4 wposition = float4(transform.xy + position * transform.zw, 0.0, 1.0);

	re.texcoord = texcoord;
	re.pos = mul(wposition, orientation);

	return re;
}