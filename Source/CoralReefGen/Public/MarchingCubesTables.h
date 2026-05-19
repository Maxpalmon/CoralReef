#pragma once

#include "CoreMinimal.h"

// Обрати внимание: убираем CORALREEFGEN_API, если это обычный C++ класс, 
// не наследуемый от UObject, чтобы избежать проблем с экспортом статических массивов
class FMarchingCubesTables
{
public:
	static const FIntVector VertexOffsets[8];
	static const int EdgeTable[256];
	static const int TriTable[256][16];
};