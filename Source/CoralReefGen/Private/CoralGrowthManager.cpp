#include "CoralGrowthManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

ACoralGrowthManager::ACoralGrowthManager() {
    PrimaryActorTick.bCanEverTick = false;
    VoxelMeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VoxelMesh"));
    RootComponent = VoxelMeshComponent;
    VoxelMeshComponent->NumCustomDataFloats = 3;
    MeshComponent = VoxelMeshComponent; 

    VoxelSize = 100.0f;
    TotalVoxelCount = 0;
    CurrentStep = 0;
    StepDelay = 0.05f;
    bIsAutoGrowth = false;
}

void ACoralGrowthManager::BeginPlay() {
    Super::BeginPlay();
    VoxelStorage = NewObject<UVoxelStorage>(this);
    InitializeSpecies(); 
    GlobalOctree = new FVoxelOctree(1024);
    InitializeEcosystemSeeds();
}

void ACoralGrowthManager::EndPlay(const EEndPlayReason::Type EndPlayReason) {
    if (GlobalOctree) { delete GlobalOctree; GlobalOctree = nullptr; }
    Super::EndPlay(EndPlayReason);
}

void ACoralGrowthManager::InitializeEcosystemSeeds() {
    if (!GlobalOctree) return;
    ActiveAgents.Empty();

    SpawnSeed(FIntVector(-10, 0, 0), 0); 
    SpawnSeed(FIntVector(10, 0, 0), 1);
    SpawnSeed(FIntVector(0, -10, 0), 2);
}

void ACoralGrowthManager::RestartSimulation() {
    UE_LOG(LogTemp, Warning, TEXT("=== ХАРД-РЕСТАРТ СИМУЛЯЦИИ ==="));
    ToggleAutoGrowth(false);

    if (GlobalOctree) { delete GlobalOctree; GlobalOctree = nullptr; }
    GlobalOctree = new FVoxelOctree(1024);

    if (VoxelMeshComponent) { VoxelMeshComponent->ClearInstances(); }
    if (VoxelStorage) { VoxelStorage = NewObject<UVoxelStorage>(this); }

    TotalVoxelCount = 0;
    CurrentStep = 0;

    InitializeEcosystemSeeds();
    ToggleAutoGrowth(true);
}

void ACoralGrowthManager::SpawnSeed(FIntVector Pos, int32 ColonyID) {
    if (!VoxelStorage || !GlobalOctree) return;

    int32 NewIdx = SpawnVoxelVisualWithReturn(Pos, ColonyID);
    GlobalOctree->SetVoxel(Pos, ColonyID, SpeciesList, VoxelMeshComponent, NewIdx);
    VoxelStorage->SetVoxel(Pos, FVoxelData(EVoxelType::Living, ColonyID));

    ActiveAgents.Add(FCoralAgent(Pos, FIntVector(0, 0, 1), ColonyID, NewIdx));
    TotalVoxelCount++;
}

void ACoralGrowthManager::SimulationStep() {
    if (!VoxelStorage || !GlobalOctree || ActiveAgents.Num() == 0) return;

    CurrentStep++;
    TArray<FCoralAgent> NextGeneration;
    TArray<FIntVector> Directions = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

    int32 TakeoverAttempts = 0;
    int32 SuccessfulTakeovers = 0;

    // 1. Анализ окружающего пространства и захват территорий
    for (const FCoralAgent& Agent : ActiveAgents) {
        TArray<FIntVector> ValidDirs;
        TMap<FIntVector, int32> TargetTakeoverInstances; 

        for (const FIntVector& Dir : Directions) {
            FIntVector NeighborPos = Agent.Position + Dir;

            if (GlobalOctree->IsOccupied(NeighborPos)) {
                int32 OtherSpecies = GlobalOctree->GetSpeciesAt(NeighborPos);
                
                if (OtherSpecies != Agent.ColonyID) {
                    if (SpeciesList.IsValidIndex(Agent.ColonyID) && SpeciesList.IsValidIndex(OtherSpecies)) {
                        
                        // Если уровень агрессии текущего вида строго выше соседа
                        if (SpeciesList[Agent.ColonyID].AggressionLevel > SpeciesList[OtherSpecies].AggressionLevel) {
                            TakeoverAttempts++;
                            
                            // Запрашиваем ID инстанса напрямую из SVO октодерева
                            int32 TargetIdx = GlobalOctree->GetVoxelInstanceIndex(NeighborPos);
                            if (TargetIdx != -1) {
                                ValidDirs.Add(Dir);
                                TargetTakeoverInstances.Add(NeighborPos, TargetIdx);
                            }
                        }
                    }
                }
                continue; 
            }

            if (VoxelStorage->GetVoxel(NeighborPos).Type != EVoxelType::Empty) continue;
            ValidDirs.Add(Dir);
        }

        // Если зажат — воксель превращается в тусклый известняк (скелет)
        if (ValidDirs.Num() == 0) {
            if (Agent.InstanceIndex != -1 && SpeciesList.IsValidIndex(Agent.ColonyID)) {
                FColor SpeciesCol = SpeciesList[Agent.ColonyID].DisplayColor;
                float DeadDim = 0.2f; 
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 0, (SpeciesCol.R / 255.0f) * DeadDim);
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 1, (SpeciesCol.G / 255.0f) * DeadDim);
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 2, (SpeciesCol.B / 255.0f) * DeadDim);
            }
            continue; 
        }

        // Сортировка направлений (Векторный фототропизм)
        ValidDirs.Sort([&](const FIntVector& A, const FIntVector& B) {
            float ScoreA = (A.Z > 0 ? PhototropismWeight : 0.0f);
            float ScoreB = (B.Z > 0 ? PhototropismWeight : 0.0f);
            return ScoreA > ScoreB;
        });

        float AdjustedGrowthProbability = (CurrentStep < 15) ? 1.0f : GrowthProbability;
        int32 MaxBranches = (FMath::FRand() < BranchingChance) ? 2 : 1;
        int32 Created = 0;
        
        for (const FIntVector& ChosenDir : ValidDirs) {
            if (Created >= MaxBranches) break;
            
            if (FMath::FRand() < AdjustedGrowthProbability) {
                FIntVector NewPos = Agent.Position + ChosenDir;

                // Если клетка содержит более слабую чужую фракцию — ПОЖИРАЕМ ЕЁ
                if (TargetTakeoverInstances.Contains(NewPos)) {
                    int32 DisplacedInstanceIdx = TargetTakeoverInstances[NewPos];

                    // Физически меняем CustomData цвета старого инстанса на наш цвет
                    FColor MyColor = SpeciesList[Agent.ColonyID].DisplayColor;
                    VoxelMeshComponent->SetCustomDataValue(DisplacedInstanceIdx, 0, MyColor.R / 255.0f);
                    VoxelMeshComponent->SetCustomDataValue(DisplacedInstanceIdx, 1, MyColor.G / 255.0f);
                    VoxelMeshComponent->SetCustomDataValue(DisplacedInstanceIdx, 2, MyColor.B / 255.0f);

                    // Обновляем октодерево (там сработает логика перезаписи)
                    GlobalOctree->SetVoxel(NewPos, Agent.ColonyID, SpeciesList, VoxelMeshComponent, DisplacedInstanceIdx);
                    VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));

                    NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID, DisplacedInstanceIdx));
                    SuccessfulTakeovers++;
                }
                else {
                    // Стандартная экспансия на пустую клетку
                    int32 NewIdx = SpawnVoxelVisualWithReturn(NewPos, Agent.ColonyID);
                    GlobalOctree->SetVoxel(NewPos, Agent.ColonyID, SpeciesList, VoxelMeshComponent, NewIdx);
                    VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));

                    NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID, NewIdx));
                    TotalVoxelCount++;
                }
                Created++;
            }
        }

        // Старение пройденного слоя
        if (Agent.InstanceIndex != -1 && SpeciesList.IsValidIndex(Agent.ColonyID)) {
            if (Created > 0) {
                FColor SpeciesCol = SpeciesList[Agent.ColonyID].DisplayColor;
                float DeadDim = 0.2f; 
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 0, (SpeciesCol.R / 255.0f) * DeadDim);
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 1, (SpeciesCol.G / 255.0f) * DeadDim);
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 2, (SpeciesCol.B / 255.0f) * DeadDim);
            } else {
                NextGeneration.Add(Agent); 
            }
        }
    }

    if (NextGeneration.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("Экосистема заблокирована. Перезапуск семян..."));
        InitializeEcosystemSeeds();
        return;
    }

    ActiveAgents = NextGeneration;

    // Точечное обновление живых верхушек с разгрузкой рендера
    for (const FCoralAgent& LiveAgent : ActiveAgents) {
        if (LiveAgent.InstanceIndex != -1 && SpeciesList.IsValidIndex(LiveAgent.ColonyID)) {
            FColor SpeciesCol = SpeciesList[LiveAgent.ColonyID].DisplayColor;
            VoxelMeshComponent->SetCustomDataValue(LiveAgent.InstanceIndex, 0, SpeciesCol.R / 255.0f);
            VoxelMeshComponent->SetCustomDataValue(LiveAgent.InstanceIndex, 1, SpeciesCol.G / 255.0f);
            VoxelMeshComponent->SetCustomDataValue(LiveAgent.InstanceIndex, 2, SpeciesCol.B / 255.0f);
            
            FTransform T;
            VoxelMeshComponent->GetInstanceTransform(LiveAgent.InstanceIndex, T, true);
            VoxelMeshComponent->UpdateInstanceTransform(LiveAgent.InstanceIndex, T, true, false, true);
        }
    }

    VoxelMeshComponent->MarkRenderStateDirty();
    
    // ОТЛАДОЧНЫЙ ДЕБАГ В ВЫВОД КОНСОЛИ
    UE_LOG(LogTemp, Warning, TEXT("--- ШАГ СИМУЛЯЦИИ: %d ---"), CurrentStep);
    UE_LOG(LogTemp, Log, TEXT("Активных полипов: %d | Всего вокселей на рифе: %d"), ActiveAgents.Num(), VoxelMeshComponent->GetInstanceCount());
    UE_LOG(LogTemp, Error, TEXT("[МЕЖВИДОВАЯ БОРЬБА] Обнаружено стыков: %d | Успешно перекрашено и сожрано: %d вокселей"), TakeoverAttempts, SuccessfulTakeovers);

    ExportStatsToCSV();
}

int32 ACoralGrowthManager::SpawnVoxelVisualWithReturn(FIntVector Pos, int32 SpeciesID) {
    if (!VoxelMeshComponent || !SpeciesList.IsValidIndex(SpeciesID)) return -1;

    FTransform T;
    T.SetLocation(FVector(Pos) * VoxelSize);
    int32 Idx = VoxelMeshComponent->AddInstance(T);
    
    FColor Col = SpeciesList[SpeciesID].DisplayColor;
    VoxelMeshComponent->SetCustomDataValue(Idx, 0, Col.R / 255.0f);
    VoxelMeshComponent->SetCustomDataValue(Idx, 1, Col.G / 255.0f);
    VoxelMeshComponent->SetCustomDataValue(Idx, 2, Col.B / 255.0f);
    
    return Idx;
}

void ACoralGrowthManager::InitializeSpecies() {
    SpeciesList.Empty();

    FSpeciesParams Acropora;
    Acropora.Name = "Acropora";
    Acropora.DisplayColor = FColor::Cyan;
    Acropora.AggressionLevel = 1; 
    SpeciesList.Add(Acropora);

    FSpeciesParams Brain;
    Brain.Name = "Brain Coral";
    Brain.DisplayColor = FColor::Orange;
    Brain.AggressionLevel = 15;
    SpeciesList.Add(Brain);

    FSpeciesParams Fire;
    Fire.Name = "Fire Coral";
    Fire.DisplayColor = FColor::Red;
    Fire.AggressionLevel = 100;
    SpeciesList.Add(Fire);
}

void ACoralGrowthManager::ToggleAutoGrowth(bool bEnable) {
    bIsAutoGrowth = bEnable;
    if (bIsAutoGrowth) {
        GetWorldTimerManager().SetTimer(GrowthTimerHandle, this, &ACoralGrowthManager::SimulationStep, StepDelay, true);
    } else {
        GetWorldTimerManager().ClearTimer(GrowthTimerHandle);
    }
}

FString ACoralGrowthManager::GetSimulationStats() {
    int32 ActiveCount = ActiveAgents.Num();
    int32 SkeletonCount = FMath::Max(0, TotalVoxelCount - ActiveCount);

    return FString::Printf(TEXT("Шаг: %d\nЖивых полипов: %d\nОбъем скелета: %d\nВсего вокселей: %d"),
        CurrentStep, ActiveCount, SkeletonCount, TotalVoxelCount);
}

void ACoralGrowthManager::ExportStatsToCSV() {
    if (!GlobalOctree) return;

    FString FilePath = FPaths::ProjectSavedDir() + TEXT("CoralBenchmark.csv");
    int64 DenseGridPotential = FMath::Pow((double)GlobalOctree->GetCurrentSize(), 3);
    int32 SVONodes = GlobalOctree->GetTotalNodesCount();

    double DenseMemoryMB = (DenseGridPotential * 4.0) / (1024.0 * 1024.0);
    double SVOMemoryMB = (SVONodes * 64.0) / (1024.0 * 1024.0);

    if (!FPaths::FileExists(FilePath)) {
        FString Header = TEXT("Step,ActualVoxels,SVONodes,DensePotential,SVOMemoryMB,DenseMemoryMB\n");
        FFileHelper::SaveStringToFile(Header, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
    }

    FString DataRow = FString::Printf(TEXT("%d,%d,%d,%lld,%.4f,%.4f\n"), 
        CurrentStep, TotalVoxelCount, SVONodes, DenseGridPotential, SVOMemoryMB, DenseMemoryMB);

    FFileHelper::SaveStringToFile(DataRow, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}