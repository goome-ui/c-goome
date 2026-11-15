Texture2D<float4> t : register(t0);

float4 vs(uint id : SV_VertexID) : SV_Position
{
    float2 uv = float2(id & 1, id >> 1);
    return float4(uv.x * 2 - 1, 1 - uv.y * 2, 0, 1);
}

float4 ps(float4 p : SV_Position) : SV_Target
{
    return t.Load(int3(p.xy, 0));
}
