#pragma once

#include "CoreMinimal.h"
#include "VoxelData.h" // Подключаем нашу структуру
#include "VoxelStorage.generated.h"

/** Структура одного чанка (блока вокселей 8x8x8) */
USTRUCT()
struct FVoxelChunk
{
    GENERATED_BODY()

    // Размер стороны блока. 8 — оптимально для баланса памяти и скорости.
    static const int32 Size = 8;

    // Массив данных вокселей (8 * 8 * 8 = 512 вокселей в блоке)
    FVoxelData Data[Size * Size * Size];
};

UCLASS(BlueprintType)
class CORALREEFGEN_API UVoxelStorage : public UObject
{
    GENERATED_BODY()

public:
    /** Хранилище: Координаты чанка (FIntVector) -> Данные чанка */
    UPROPERTY()
    TMap<FIntVector, FVoxelChunk> ChunkMap;

    /** Установить воксель по глобальным координатам */
    UFUNCTION(BlueprintCallable, Category = "CoralVoxel")
    void SetVoxel(FIntVector WorldPos, FVoxelData NewData);

    /** Получить воксель по глобальным координатам */
    UFUNCTION(BlueprintCallable, Category = "CoralVoxel")
    FVoxelData GetVoxel(FIntVector WorldPos);

private:
    /** Перевод мировых координат в координаты чанка */
    FIntVector WorldToChunkCoords(FIntVector WorldPos) const;

    /** Перевод мировых координат в индекс внутри массива чанка */
    int32 WorldToIndex(FIntVector WorldPos) const;
};