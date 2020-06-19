struct CBPerDraw
{
    float4x4 worldViewProj;
    float4 color;
};

CBPerDraw preDrawBuffer : register(b0);

struct PSInput
{
    float4 position : SV_POSITION;
};

struct VSInput
{
    float3 position : POSITION;
};

PSInput VS(VSInput input)
{
    PSInput result;
    result.position = mul(float4(input.position, 1.0f), preDrawBuffer.worldViewProj);
    return result;
}

float4 PS(PSInput input) : SV_TARGET
{
    return preDrawBuffer.color;
}
