// MarchingCubes.compute.hlsl

cbuffer VolumeParams : register(b0)
{
    int voxelResX;
    int voxelResY;
    int voxelResZ;
    float isoLevel;
};

StructuredBuffer<uint> volumeData : register(t0);
RWStructuredBuffer<float3> vertexBuffer : register(u0);
RWStructuredBuffer<uint3> triangleBuffer : register(u1);
RWByteAddressBuffer counterBuffer : register(u2);

int index(int x, int y, int z)
{
    return x + y * voxelResX + z * voxelResX * voxelResY;
}

float sample(int x, int y, int z)
{
    return volumeData[index(x, y, z)] / 255.0;
}

[numthreads(4, 4, 4)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    int x = id.x;
    int y = id.y;
    int z = id.z;
    if (x >= voxelResX - 1 || y >= voxelResY - 1 || z >= voxelResZ - 1)
        return;

    float3 p[8];
    float val[8];
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        int xi = x + (i & 1);
        int yi = y + ((i >> 1) & 1);
        int zi = z + ((i >> 2) & 1);
        val[i] = sample(xi, yi, zi);
        p[i] = float3(xi, yi, zi);
    }

    int cubeIndex = 0;
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        if (val[i] < isoLevel)
            cubeIndex |= (1 << i);
    }

    if (cubeIndex == 0 || cubeIndex == 255)
        return;

    uint triID;
    counterBuffer.InterlockedAdd(0, 1, triID);
    float3 center = (p[0] + p[6]) * 0.5;
    vertexBuffer[triID * 3 + 0] = center + float3(0.0, 0.0, 0.0);
    vertexBuffer[triID * 3 + 1] = center + float3(0.5, 0.0, 0.0);
    vertexBuffer[triID * 3 + 2] = center + float3(0.0, 0.5, 0.0);
    triangleBuffer[triID] = uint3(triID * 3, triID * 3 + 1, triID * 3 + 2);
}
