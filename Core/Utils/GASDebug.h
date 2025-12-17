#pragma once
#include <iostream>
#include <filesystem>
#include "GASAssetManager.h"
#include "../Types/GASConfig.h"

namespace fs = std::filesystem;

void EnsureDirectoriesExist();

bool RunImportTest(const std::string& SourceFBX);