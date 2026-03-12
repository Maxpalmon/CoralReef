#include "CoralGrowthManager.h"

ACoralGrowthManager::ACoralGrowthManager()
{
    PrimaryActorTick.bCanEverTick = false;

    // Создаем компонент для визуализации кубиков
    MeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("VoxelMesh"));
    RootComponent = MeshComponent;

    // Настройки оптимизации
    MeshComponent->SetMobility(EComponentMobility::Static);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACoralGrowthManager::BeginPlay()
{
    Super::BeginPlay();

    // Инициализируем хранилище
    VoxelStorage = NewObject<UVoxelStorage>(this);
}

void ACoralGrowthManager::SpawnSeed(FIntVector Pos, int32 ColonyID)
{
    if (!VoxelStorage) return;

    // Создаем данные вокселя
    FVoxelData SeedData;
    SeedData.Type = EVoxelType::Living;
    SeedData.ColonyID = ColonyID;

    // Сохраняем в память
    VoxelStorage->SetVoxel(Pos, SeedData);
    ActiveAgents.Add(FCoralAgent(Pos, ColonyID));

    // Отрисовываем первый кубик
    FTransform T;
    T.SetLocation(FVector(Pos) * VoxelSize);
    MeshComponent->AddInstance(T);
}

void ACoralGrowthManager::SimulationStep()
{
    if (!VoxelStorage || ActiveAgents.Num() == 0) return;

    TArray<FCoralAgent> NextGeneration;
    TArray<FIntVector> Directions = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };

    // Проходим по всем активным точкам
    for (const FCoralAgent& Agent : ActiveAgents)
    {
        for (const FIntVector& Dir : Directions)
        {
            FIntVector NeighborPos = Agent.Position + Dir;

            // Если воксель пустой
            if (VoxelStorage->GetVoxel(NeighborPos).Type == EVoxelType::Empty)
            {
                // Случайный шанс роста
                if (FMath::FRand() < GrowthProbability)
                {
                    FVoxelData NewData(EVoxelType::Living, Agent.ColonyID);
                    VoxelStorage->SetVoxel(NeighborPos, NewData);

                    NextGeneration.Add(FCoralAgent(NeighborPos, Agent.ColonyID));

                    // Визуализируем кубик
                    FTransform T;
                    T.SetLocation(FVector(NeighborPos) * VoxelSize);
                    MeshComponent->AddInstance(T);
                }
            }
        }
    }

    // Обновляем список агентов (в этой версии кораллы растут облаком)
    for (const FCoralAgent& NewAgent : NextGeneration)
    {
        ActiveAgents.Add(NewAgent);
    }
}