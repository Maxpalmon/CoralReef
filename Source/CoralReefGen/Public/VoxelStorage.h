#pragma once

#include "CoreMinimal.h"
#include "VoxelData.h" 
#include "VoxelStorage.generated.h"

//Chunk structure
USTRUCT()
struct FVoxelChunk
{
    GENERATED_BODY()
    
    static const int32 Size = 8;

    //Voxel Data Array
    FVoxelData Data[Size * Size * Size];
};

UCLASS(BlueprintType)
class CORALREEFGEN_API UVoxelStorage : public UObject
{
    GENERATED_BODY()

public:
    //Chunk coordinates
    UPROPERTY()
    TMap<FIntVector, FVoxelChunk> ChunkMap;

    //Set voxel by global coordinates
    UFUNCTION(BlueprintCallable, Category = "CoralVoxel")
    void SetVoxel(FIntVector WorldPos, FVoxelData NewData);

    //Get voxel by global coordinates
    UFUNCTION(BlueprintCallable, Category = "CoralVoxel")
    FVoxelData GetVoxel(FIntVector WorldPos);

private:
    //Transform world coordinates in chunk coordinates
    FIntVector WorldToChunkCoords(FIntVector WorldPos) const;

    //Transform world coordinates in index inside chunk array
    int32 WorldToIndex(FIntVector WorldPos) const;
};