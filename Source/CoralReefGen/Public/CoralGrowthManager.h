#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "VoxelStorage.h"
#include "CoralGrowthManager.generated.h"

USTRUCT(BlueprintType)
struct FCoralAgent
{
    GENERATED_BODY()

    UPROPERTY()
    FIntVector Position;

    UPROPERTY()
    FIntVector Direction;

    UPROPERTY()
    int32 ColonyID;

    FCoralAgent() : Position(0, 0, 0), Direction(0, 0, 1), ColonyID(0) {}
    FCoralAgent(FIntVector Pos, FIntVector Dir, int32 ID)
        : Position(Pos), Direction(Dir), ColonyID(ID) {
    }
};

UCLASS()
class CORALREEFGEN_API ACoralGrowthManager : public AActor
{
    GENERATED_BODY()

public:
    ACoralGrowthManager();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coral|Visual")
    UInstancedStaticMeshComponent* MeshComponent;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Data")
    UVoxelStorage* VoxelStorage;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Simulation")
    TArray<FCoralAgent> ActiveAgents;

    // --- œ¿–¿Ã≈“–€ —“¿“»—“» » ---
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 TotalVoxelCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 CurrentStep = 0;

    // --- œ¿–¿Ã≈“–€ –Œ—“¿ ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    float GrowthProbability = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    float PhototropismWeight = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    float InertiaWeight = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    float BranchingChance = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    int32 MaxDensity = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Settings")
    float VoxelSize = 100.0f;

    // --- Ã≈“Œƒ€ ---
    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SimulationStep();

    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SpawnSeed(FIntVector Pos, int32 ColonyID);

    UFUNCTION(BlueprintCallable, Category = "Coral|Stats")
    FString GetSimulationStats();

protected:
    virtual void BeginPlay() override;
};