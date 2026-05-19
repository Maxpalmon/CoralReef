#include "CoralGrowthManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include <chrono>

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

    //SpawnSeed(FIntVector(-10, 0, 0), 0); 
    //SpawnSeed(FIntVector(10, 0, 0), 1);
    //SpawnSeed(FIntVector(0, -10, 0), 2);
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
    // Базовая проверка валидности указателей и наличия активных полипов
    if (!VoxelStorage || !GlobalOctree || ActiveAgents.Num() == 0) return;
    
    // --- СТАРТ ЗАМЕРА ВРЕМЕНИ ---
    auto StartTime = std::chrono::high_resolution_clock::now();

    CurrentStep++;
    TArray<FCoralAgent> NextGeneration;
    
    // 6 возможных направлений смещения в 3D пространстве (окрестность фон Неймана)
    TArray<FIntVector> Directions = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

    int32 TakeoverAttempts = 0;
    int32 SuccessfulTakeovers = 0;

    // Безопасное приведение коэффициентов для предотвращения деления на ноль или некорректных весов
    float SafePhototropism = FMath::Max(0.0f, PhototropismWeight);
    float SafeInertia = FMath::Max(0.0f, InertiaWeight);

    // Нормализуем твой вектор CurrentDirection, чтобы он работал корректно при скалярном умножении
    FVector TargetResourceVector = CurrentDirection.GetSafeNormal();

    // 1. АНАЛИЗ ОКРУЖАЮЩЕГО ПРОСТРАНСТВА И МЕЖВИДОВАЯ БОРЬБА
    for (const FCoralAgent& Agent : ActiveAgents) {
        
        // --- СИМУЛЯЦИЯ ОСВЕЩЕННОСТИ (LightThreshold) ---
        // Пускаем виртуальный луч вертикально вверх по октодереву для проверки самозатенения рифа
        float CalculatedLight = 1.0f;
        int32 ShadowRayLength = 15; // Глубина проверки геометрии над агентом
        int32 BlockedLayers = 0;
        
        for (int32 i = 1; i <= ShadowRayLength; ++i) {
            FIntVector CheckRayPos = Agent.Position + FIntVector(0, 0, i);
            if (GlobalOctree->IsOccupied(CheckRayPos)) {
                BlockedLayers++;
            }
        }
        
        // Каждый блокирующий слой вокселей сверху уменьшает количество света на 15%
        CalculatedLight = FMath::Max(0.0f, 1.0f - (BlockedLayers * 0.15f));

        // Если свет упал ниже заданного порога, полип "засыпает" (не размножается) и кальцифицируется
        if (CalculatedLight < LightThreshold) {
            if (Agent.InstanceIndex != -1 && SpeciesList.IsValidIndex(Agent.ColonyID)) {
                FColor SpeciesCol = SpeciesList[Agent.ColonyID].DisplayColor;
                float DeadDim = 0.2f; // Коэффициент затухания цвета для скелета
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 0, (SpeciesCol.R / 255.0f) * DeadDim);
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 1, (SpeciesCol.G / 255.0f) * DeadDim);
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 2, (SpeciesCol.B / 255.0f) * DeadDim);
            }
            continue; // Агент исключается из репродуктивного цикла, потомство не генерируется
        }
        // ------------------------------------------------

        TArray<FIntVector> ValidDirs;
        TMap<FIntVector, int32> TargetTakeoverInstances; 

        // Сканируем 6 направлений вокруг текущего агента
        for (const FIntVector& Dir : Directions) {
            FIntVector NeighborPos = Agent.Position + Dir;

            // Если ячейка занята — проверяем возможность межвидового захвата (агрессия)
            if (GlobalOctree->IsOccupied(NeighborPos)) {
                int32 OtherSpecies = GlobalOctree->GetSpeciesAt(NeighborPos);
                
                if (OtherSpecies != Agent.ColonyID) {
                    if (SpeciesList.IsValidIndex(Agent.ColonyID) && SpeciesList.IsValidIndex(OtherSpecies)) {
                        
                        // Если уровень агрессии текущего вида строго выше уровня жертвы
                        if (SpeciesList[Agent.ColonyID].AggressionLevel > SpeciesList[OtherSpecies].AggressionLevel) {
                            TakeoverAttempts++;
                            
                            // Получаем индекс инстанса из SVO для последующего перекрашивания
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

            // Если ячейка занята внутренней логикой VoxelStorage, но не октодеревом (защита от дублирования)
            if (VoxelStorage->GetVoxel(NeighborPos).Type != EVoxelType::Empty) continue;
            
            ValidDirs.Add(Dir);
        }

        // Если полип зажат со всех сторон и расти некуда — переводим его в состояние твердого скелета
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

        // --- МАТЕМАТИЧЕСКАЯ СОРТИРОВКА НАПРАВЛЕНИЙ (CurrentDirection + Инерция) ---
        ValidDirs.Sort([&](const FIntVector& A, const FIntVector& B) {
            FVector VecA = FVector(A.X, A.Y, A.Z).GetSafeNormal();
            FVector VecB = FVector(B.X, B.Y, B.Z).GetSafeNormal();

            // 1. Эффект Инерции: вычисляем скалярное произведение с вектором ПРОШЛОГО шага агента
            FVector OldDir = FVector(Agent.Direction.X, Agent.Direction.Y, Agent.Direction.Z).GetSafeNormal();
            float InertiaScoreA = FVector::DotProduct(VecA, OldDir) * SafeInertia;
            float InertiaScoreB = FVector::DotProduct(VecB, OldDir) * SafeInertia;

            // 2. Эффект Аттрактора: скалярное произведение с твоим супер-вектором CurrentDirection
            float TargetScoreA = FVector::DotProduct(VecA, TargetResourceVector) * SafePhototropism;
            float TargetScoreB = FVector::DotProduct(VecB, TargetResourceVector) * SafePhototropism;

            // Итоговый вес: влияние вектора ресурсов + инерция движения + легкий природный шум (органика)
            float TotalA = TargetScoreA + InertiaScoreA + FMath::FRandRange(0.0f, 0.05f);
            float TotalB = TargetScoreB + InertiaScoreB + FMath::FRandRange(0.0f, 0.05f);

            return TotalA > TotalB;
        });
        // -------------------------------------------------------------------------

        // 2. РАСШЕРЕНИЕ И СУПЕРПОЗИЦИЯ РОСТА
        float AdjustedGrowthProbability = (CurrentStep < 15) ? 1.0f : GrowthProbability;
        int32 MaxBranches = (FMath::FRand() < BranchingChance) ? 2 : 1;
        int32 Created = 0;
        
        for (const FIntVector& ChosenDir : ValidDirs) {
            if (Created >= MaxBranches) break;
            
            if (FMath::FRand() < AdjustedGrowthProbability) {
                FIntVector NewPos = Agent.Position + ChosenDir;

                // СЛУЧАЙ А: Захват чужой территории (Пожирание)
                if (TargetTakeoverInstances.Contains(NewPos)) {
                    int32 DisplacedInstanceIdx = TargetTakeoverInstances[NewPos];

                    // Перекрашиваем инстанс побежденного коралла в наш цвет через CustomData
                    FColor MyColor = SpeciesList[Agent.ColonyID].DisplayColor;
                    VoxelMeshComponent->SetCustomDataValue(DisplacedInstanceIdx, 0, MyColor.R / 255.0f);
                    VoxelMeshComponent->SetCustomDataValue(DisplacedInstanceIdx, 1, MyColor.G / 255.0f);
                    VoxelMeshComponent->SetCustomDataValue(DisplacedInstanceIdx, 2, MyColor.B / 255.0f);

                    // Обновляем данные в структурах памяти (перезаписываем ID фракции)
                    GlobalOctree->SetVoxel(NewPos, Agent.ColonyID, SpeciesList, VoxelMeshComponent, DisplacedInstanceIdx);
                    VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));

                    // Передаем ChosenDir как новое направление движения (Agent.Direction) для следующего шага
                    NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID, DisplacedInstanceIdx));
                    SuccessfulTakeovers++;
                }
                // СЛУЧАЙ Б: Экспансия на свободную ячейку пространства
                else {
                    int32 NewIdx = SpawnVoxelVisualWithReturn(NewPos, Agent.ColonyID);
                    GlobalOctree->SetVoxel(NewPos, Agent.ColonyID, SpeciesList, VoxelMeshComponent, NewIdx);
                    VoxelStorage->SetVoxel(NewPos, FVoxelData(EVoxelType::Living, Agent.ColonyID));

                    // Передаем ChosenDir новому полипу
                    NextGeneration.Add(FCoralAgent(NewPos, ChosenDir, Agent.ColonyID, NewIdx));
                    TotalVoxelCount++;
                }
                Created++;
            }
        }

        // Старение родительского слоя: если он дал потомство, то покрывается внутренней коркой и тускнеет
        if (Agent.InstanceIndex != -1 && SpeciesList.IsValidIndex(Agent.ColonyID)) {
            if (Created > 0) {
                FColor SpeciesCol = SpeciesList[Agent.ColonyID].DisplayColor;
                float DeadDim = 0.2f; 
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 0, (SpeciesCol.R / 255.0f) * DeadDim);
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 1, (SpeciesCol.G / 255.0f) * DeadDim);
                VoxelMeshComponent->SetCustomDataValue(Agent.InstanceIndex, 2, (SpeciesCol.B / 255.0f) * DeadDim);
            } else {
                // Если на этом шаге родитель не смог размножиться, он остается активным агентом на следующий такт
                NextGeneration.Add(Agent); 
            }
        }
    }

    // Если на рифе не осталось ни одного активного полипа — перезапускаем систему из семян
    if (NextGeneration.Num() == 0) {
        UE_LOG(LogTemp, Warning, TEXT("Экосистема заблокирована. Перезапуск семян..."));
        InitializeEcosystemSeeds();
        return;
    }

    // Обновляем список активных фронтов роста
    ActiveAgents = NextGeneration;

    // 3. СВЕРХБЫСТРОЕ ОБНОВЛЕНИЕ ТРАНСФОРМОВ ДЛЯ ЖИВЫХ ВЕРХУШЕК РИФА
    for (const FCoralAgent& LiveAgent : ActiveAgents) {
        if (LiveAgent.InstanceIndex != -1 && SpeciesList.IsValidIndex(LiveAgent.ColonyID)) {
            // Поддерживаем максимальную яркость цвета для живых полипов на концах веток
            FColor SpeciesCol = SpeciesList[LiveAgent.ColonyID].DisplayColor;
            VoxelMeshComponent->SetCustomDataValue(LiveAgent.InstanceIndex, 0, SpeciesCol.R / 255.0f);
            VoxelMeshComponent->SetCustomDataValue(LiveAgent.InstanceIndex, 1, SpeciesCol.G / 255.0f);
            VoxelMeshComponent->SetCustomDataValue(LiveAgent.InstanceIndex, 2, SpeciesCol.B / 255.0f);
            
            FTransform T;
            VoxelMeshComponent->GetInstanceTransform(LiveAgent.InstanceIndex, T, true);
            // Точечно пушим изменения инстанса в графический конвейер без полной пересборки буфера
            VoxelMeshComponent->UpdateInstanceTransform(LiveAgent.InstanceIndex, T, true, false, true);
        }
    }

    // Сигнализируем рендеру, что данные Custom Data или трансформы обновились
    VoxelMeshComponent->MarkRenderStateDirty();
    
    // --- КОНЕЦ ЗАМЕРА ВРЕМЕНИ ---
    auto EndTime = std::chrono::high_resolution_clock::now();
    
    // Считаем длительность в миллисекундах (дробное число для максимальной точности)
    float StepDurationMS = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
    
    // ОТЛАДОЧНЫЙ ДЕБАГ (добавим время в консоль)
    UE_LOG(LogTemp, Warning, TEXT("--- ШАГ СИМУЛЯЦИИ: %d ---"), CurrentStep);
    UE_LOG(LogTemp, Log, TEXT("Активных полипов: %d | Всего вокселей на рифе: %d"), ActiveAgents.Num(), VoxelMeshComponent->GetInstanceCount());
    UE_LOG(LogTemp, Log, TEXT("Время выполнения шага: %.4f мс"), StepDurationMS);
    UE_LOG(LogTemp, Error, TEXT("[МЕЖВИДОВАЯ БОРЬБА] Обнаружено стыков: %d | Успешно перекрашено и сожрано: %d вокселей"), TakeoverAttempts, SuccessfulTakeovers);
    
    // ВЫВОД СТАТИСТИКИ В КОНСОЛЬ И ЛОГИ ДЛЯ ОТЛАДКИ
    UE_LOG(LogTemp, Warning, TEXT("--- ШАГ СИМУЛЯЦИИ: %d ---"), CurrentStep);
    UE_LOG(LogTemp, Log, TEXT("Активных полипов: %d | Всего вокселей на рифе: %d"), ActiveAgents.Num(), VoxelMeshComponent->GetInstanceCount());
    UE_LOG(LogTemp, Error, TEXT("[МЕЖВИДОВАЯ БОРЬБА] Обнаружено стыков: %d | Успешно перекрашено и сожрано: %d вокселей"), TakeoverAttempts, SuccessfulTakeovers);

    // Запись данных в CSV для твоего классного бенчмарка производительности
    ExportStatsToCSV(StepDurationMS);
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

void ACoralGrowthManager::ExportStatsToCSV(float StepTimeMS) {
    FString FilePath = FPaths::ProjectSavedDir() / TEXT("CoralBenchmark.csv");
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    FString FileContent;

    // Если файл еще не существует, создаем его и пишем заголовки (добавляем StepTimeMS в конец)
    if (!PlatformFile.FileExists(*FilePath)) {
        FileContent = TEXT("Step,ActualVoxels,SVONodes,DensePotential,SVOMemoryMB,DenseMemoryMB,StepTimeMS\n");
    }

    // Собираем строку данных для текущего шага
    int32 ActualVoxels = VoxelMeshComponent->GetInstanceCount();
    int32 NodesCount = GlobalOctree->GetTotalNodesCount(); // Твой метод подсчета узлов SVO
    int32 DensePotential = 1073741824; // Твоя константа 1024^3
    float SVOMemory = (NodesCount * 64.0) / (1024.0 * 1024.0);
    float DenseMemory = 4096.0f;

    // Добавляем новую строчку. %.4f сохранит время с 4 знаками после запятой
    FileContent += FString::Printf(TEXT("%d,%d,%d,%d,%.4f,%.4f,%.4f\n"), 
        CurrentStep, 
        ActualVoxels, 
        NodesCount, 
        DensePotential, 
        SVOMemory, 
        DenseMemory, 
        StepTimeMS
    );

    // Дописываем строку в файл (или создаем новый, если его не было)
    FFileHelper::SaveStringToFile(FileContent, *FilePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}