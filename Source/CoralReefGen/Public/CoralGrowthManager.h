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

    // Добавляем индекс инстанса, чтобы знать, какой кубик перекрашивать
    UPROPERTY()
    int32 InstanceIndex;

    FCoralAgent() : Position(0, 0, 0), Direction(0, 0, 1), ColonyID(0), InstanceIndex(-1) {}
    FCoralAgent(FIntVector Pos, FIntVector Dir, int32 ID, int32 InIndex)
        : Position(Pos), Direction(Dir), ColonyID(ID), InstanceIndex(InIndex) {
    }
};

UCLASS()
class CORALREEFGEN_API ACoralGrowthManager : public AActor
{
    GENERATED_BODY()

public:
    ACoralGrowthManager();

    // Добавь это в секцию public:

// --- ТАЙМЕР ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Automation")
    bool bIsAutoGrowth = false; // Вкл/выкл авто-рост

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Automation")
    float StepDelay = 0.1f; // Задержка между шагами

    FTimerHandle GrowthTimerHandle;

    // --- ОПТИМИЗАЦИЯ (СВЕТ) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    int32 LightThreshold = 2; // Если сверху больше 2-х соседей, полип "затенен" и умирает

    // --- ЭКСПОРТ ---
    UFUNCTION(BlueprintCallable, Category = "Coral|Data")
    void ExportStatsToCSV();

    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void ToggleAutoGrowth(bool bEnable);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    FVector CurrentDirection = FVector(1.0f, 0.0f, 0.0f); // Направление течения

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CurrentStrength = 0.2f; // Сила влияния течения

    // --- НОВАЯ СТАТИСТИКА ---
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 SkeletonVoxelCount = 0; // Объем скелета (мертвые воксели)

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 LivingPolypCount = 0; // Количество живых полипов

    // Визуализация живых и мертвых полипов
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Visual")
    FLinearColor LivingColor = FLinearColor(0.1f, 0.8f, 0.2f); // Зеленый (живой)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Visual")
    FLinearColor SkeletonColor = FLinearColor(0.8f, 0.8f, 0.8f); // Серый (скелет)


    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coral|Visual")
    UInstancedStaticMeshComponent* MeshComponent;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Data")
    UVoxelStorage* VoxelStorage;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Simulation")
    TArray<FCoralAgent> ActiveAgents;

    // --- ПАРАМЕТРЫ СТАТИСТИКИ ---
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 TotalVoxelCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 CurrentStep = 0;

    // --- ПАРАМЕТРЫ РОСТА ---
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

    // --- МЕТОДЫ ---
    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SimulationStep();

    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SpawnSeed(FIntVector Pos, int32 ColonyID);

    UFUNCTION(BlueprintCallable, Category = "Coral|Stats")
    FString GetSimulationStats();

protected:
    virtual void BeginPlay() override;
};