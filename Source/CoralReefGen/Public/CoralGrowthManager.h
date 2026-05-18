#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "VoxelStorage.h"
#include "FVoxelOctree.h"
#include "CoralGrowthManager.generated.h"

USTRUCT(BlueprintType)
struct FSpeciesParams {
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral")
    FColor DisplayColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral", meta = (ClampMin = "0", ClampMax = "100"))
    int32 AggressionLevel; // Сила вида для механики Takeover (0..100)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral")
    float Alpha; // Коэффициент инерции

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral")
    float Beta;  // Коэффициент фототропизма

    FSpeciesParams() 
        : Name("Default Species")
        , DisplayColor(FColor::Green)
        , AggressionLevel(1)
        , Alpha(0.5f)
        , Beta(0.5f) 
    {}
};

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

    UPROPERTY()
    int32 InstanceIndex;

    FCoralAgent() : Position(0, 0, 0), Direction(0, 0, 1), ColonyID(0), InstanceIndex(-1) {}
    FCoralAgent(FIntVector Pos, FIntVector Dir, int32 ID, int32 InIndex)
        : Position(Pos), Direction(Dir), ColonyID(ID), InstanceIndex(InIndex) {}
};

UCLASS()
class CORALREEFGEN_API ACoralGrowthManager : public AActor
{
    GENERATED_BODY()

public:
    ACoralGrowthManager();

    // Автоматический рост
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Automation")
    bool bIsAutoGrowth = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Automation")
    float StepDelay = 0.05f;

    // Управление таймером (перенесено полностью в public, дубликат из private удален)
    FTimerHandle GrowthTimerHandle;

    // Ограничения и оптимизация освещения
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    int32 LightThreshold = 2; 

    // Вектор текущего направления течения/роста по умолчанию
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    FVector CurrentDirection = FVector(0.0f, 0.0f, 1.0f); 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CurrentStrength = 0.2f;

    // Глобальные параметры симуляции (если не переопределены в Species)
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    TArray<FSpeciesParams> SpeciesList;

    // Визуализация
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coral|Visual")
    UInstancedStaticMeshComponent* VoxelMeshComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coral|Visual")
    UInstancedStaticMeshComponent* MeshComponent; // Синхронизированный указатель для обратной совместимости

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Visual")
    FLinearColor LivingColor = FLinearColor(0.1f, 0.8f, 0.2f); 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Visual")
    FLinearColor SkeletonColor = FLinearColor(0.8f, 0.8f, 0.8f);

    // Данные и статистика
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Data")
    UVoxelStorage* VoxelStorage;
    
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 TotalVoxelCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 CurrentStep = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 SkeletonPolypCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 LivingPolypCount = 0;

    // Методы управления симуляцией
    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SimulationStep();

    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void ToggleAutoGrowth(bool bEnable);
    
    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void RestartSimulation();
    
    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SpawnSeed(FIntVector Pos, int32 ColonyID);

    UFUNCTION(BlueprintCallable, Category = "Coral|Data")
    void ExportStatsToCSV();

    UFUNCTION(BlueprintCallable, Category = "Coral|Stats")
    FString GetSimulationStats();

    void InitializeEcosystemSeeds();

protected:
    virtual void BeginPlay() override;
    
    // Исправлено: добавлен const и override для корректного освобождения памяти SVO
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Simulation")
    TArray<FCoralAgent> ActiveAgents;
    
    int32 SpawnVoxelVisualWithReturn(FIntVector Pos, int32 SpeciesID);

private:
    void InitializeSpecies();
    
    FVoxelOctree* GlobalOctree; 
};