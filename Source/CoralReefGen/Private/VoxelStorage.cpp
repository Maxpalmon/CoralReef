#include "VoxelStorage.h"

FIntVector UVoxelStorage::WorldToChunkCoords(FIntVector WorldPos) const
{
    // Используем побитовый сдвиг или деление для определения координат чанка
    auto FloorDiv = [](int32 a, int32 b) {
        return (a >= 0) ? (a / b) : ((a - b + 1) / b);
        };
    return FIntVector(FloorDiv(WorldPos.X, 8), FloorDiv(WorldPos.Y, 8), FloorDiv(WorldPos.Z, 8));
}

int32 UVoxelStorage::WorldToIndex(FIntVector WorldPos) const
{
    // Получаем локальные координаты внутри чанка (0-7)
    auto GetLocal = [](int32 pos) {
        int32 res = pos % 8;
        return (res < 0) ? (res + 8) : res;
        };

    int32 lx = GetLocal(WorldPos.X);
    int32 ly = GetLocal(WorldPos.Y);
    int32 lz = GetLocal(WorldPos.Z);

    return lx + (ly * 8) + (lz * 8 * 8);
}

void UVoxelStorage::SetVoxel(FIntVector WorldPos, FVoxelData NewData)
{
    FIntVector ChunkCoords = WorldToChunkCoords(WorldPos);

    // Если чанка нет, TMap создаст его автоматически при обращении через FindOrAdd
    FVoxelChunk& Chunk = ChunkMap.FindOrAdd(ChunkCoords);
    Chunk.Data[WorldToIndex(WorldPos)] = NewData;
}

FVoxelData UVoxelStorage::GetVoxel(FIntVector WorldPos)
{
    FIntVector ChunkCoords = WorldToChunkCoords(WorldPos);

    if (FVoxelChunk* Chunk = ChunkMap.Find(ChunkCoords))
    {
        return Chunk->Data[WorldToIndex(WorldPos)];
    }

    return FVoxelData(); // Возвращаем пустой воксель, если чанк не найден
}