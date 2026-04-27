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
    
    UPROPERTY(EditAnywhere, Category = "Coral")
    FString Name;

    UPROPERTY(EditAnywhere, Category = "Coral")
    FColor DisplayColor;

    UPROPERTY(EditAnywhere, Category = "Coral", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Aggression; // Шанс вытеснить соседа

    FSpeciesParams() : Name("Default"), DisplayColor(FColor::Green), Aggression(0.5f) {}
    
    UPROPERTY(EditAnywhere) float Alpha; // Инерция
    UPROPERTY(EditAnywhere) float Beta;  // Свет
    UPROPERTY(EditAnywhere) int32 AggressionLevel; // Сила вида
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

    // Add InstanceIndex parameter for coloring voxel information
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

// Timer
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Automation")
    bool bIsAutoGrowth = false; //Switching autogrowth

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Automation")
    float StepDelay = 0.1f; //Delay between steps

    FTimerHandle GrowthTimerHandle;

    // Light optimisation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    int32 LightThreshold = 2; // If there are more then 2 neighbors with voxel, polyp is in shadow and dies

    // Export
    UFUNCTION(BlueprintCallable, Category = "Coral|Data")
    void ExportStatsToCSV();

    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void ToggleAutoGrowth(bool bEnable);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    FVector CurrentDirection = FVector(1.0f, 0.0f, 0.0f); 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CurrentStrength = 0.2f; //Strength of flaw impact

    // Statistics
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 SkeletonPolypCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 LivingPolypCount = 0;

    //Visualisation of live and dead polyps
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Visual")
    FLinearColor LivingColor = FLinearColor(0.1f, 0.8f, 0.2f); 

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Visual")
    FLinearColor SkeletonColor = FLinearColor(0.8f, 0.8f, 0.8f);
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coral|Growth Settings")
    TArray<FSpeciesParams> SpeciesList;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Coral|Visual")
    UInstancedStaticMeshComponent* MeshComponent;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Data")
    UVoxelStorage* VoxelStorage;
    
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 TotalVoxelCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Coral|Stats")
    int32 CurrentStep = 0;
    
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
    
    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SimulationStep();

    UFUNCTION(BlueprintCallable, Category = "Coral|Methods")
    void SpawnSeed(FIntVector Pos, int32 ColonyID);

    UFUNCTION(BlueprintCallable, Category = "Coral|Stats")
    FString GetSimulationStats();

protected:
    virtual void BeginPlay() override;
    
    void EndPlay(EEndPlayReason::Type EndPlayReason);
    
    // Метод для отрисовки вокселя
    void SpawnVoxelVisual(FIntVector Pos, int32 SpeciesID);

        
    UPROPERTY(BlueprintReadOnly, Category = "Coral|Simulation")
    TArray<FCoralAgent> ActiveAgents;
    
    // Убедись, что компонент объявлен
    UPROPERTY(VisibleAnywhere, Category = "Coral|Visual")
    UInstancedStaticMeshComponent* VoxelMeshComponent;
    
private:
    void InitializeSpecies();
    
    // В класс менеджера
    //TArray<FSpeciesParams> SpeciesList;
    FVoxelOctree* GlobalOctree; // Параллельное хранилище для SVO
};

