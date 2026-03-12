#pragma once

#include "CoreMinimal.h"
#include "VoxelData.generated.h"

UENUM(BlueprintType)
enum class EVoxelType : uint8
{
    Empty       UMETA(DisplayName = "Empty"),
    Living      UMETA(DisplayName = "Living"),
    Sedimented  UMETA(DisplayName = "Sedimented"),
    Substrate   UMETA(DisplayName = "Substrate")
};

USTRUCT(BlueprintType)
struct FVoxelData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    EVoxelType Type = EVoxelType::Empty;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    int32 ColonyID = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel")
    float Density = 0.0f;

    FVoxelData() {}
    FVoxelData(EVoxelType InType, int32 InColonyID)
        : Type(InType), ColonyID(InColonyID), Density(1.0f) {
    }
};