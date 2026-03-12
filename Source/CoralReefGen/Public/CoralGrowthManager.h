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

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntVector Position;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ColonyID;

    FCoralAgent() : Position(0, 0, 0), ColonyID(0) {}
    FCoralAgent(FIntVector Pos, int32 ID) : Position(Pos), ColonyID(ID) {}
};

UCLASS()
class CORALREEFGEN_API ACoralGrowthManager : public AActor
{
    GENERATED_BODY()

public:
    ACoralGrowthManager();

    // Компонент для отрисовки кубиков (вокселей)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coral|Visual")
    UInstancedStaticMeshComponent* MeshComponent;

    // Наше хранилище вокселей
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Data")
    UVoxelStorage* VoxelStorage;

    // Список активных точек роста (агентов)
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Simulation")
    TArray<FCoralAgent> ActiveAgents;

    // Параметры симуляции
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Settings")
    float GrowthProbability = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Settings")
    float VoxelSize = 100.0f;

    // Сделать один шаг симуляции
    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SimulationStep();

    // Начальная инициализация (посадка «семени» коралла)
    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SpawnSeed(FIntVector Pos, int32 ColonyID);

protected:
    virtual void BeginPlay() override;
};