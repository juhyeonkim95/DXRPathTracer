struct VS_OUTPUT
{
    float4 pos: SV_POSITION;
    float2 texCoord: TEXCOORD;
};

VS_OUTPUT main(float3 pos : POSITION)
{
    VS_OUTPUT output;
    output.pos = float4(pos, 1.0f);
    float2 texC = float2(pos.x, -pos.y);
    texC = (texC + 1.0f) * 0.5f;
    output.texCoord = texC;

    //output.texCoord = (pos.xy + 1.0f) * 0.5f;
    //output.texCoord.y = 1 - output.texCoord.y;
    return output;
}