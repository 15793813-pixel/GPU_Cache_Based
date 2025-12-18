#pragma once
#include <iostream>
#include <filesystem>
#include "GASAssetManager.h"
#include "../Types/GASConfig.h"
#include <assimp/Logger.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>
namespace fs = std::filesystem;

void EnsureDirectoriesExist();

bool RunImportTest(const std::string& SourceFBX);

class GASAssimpLogStream : public Assimp::LogStream
{
public:
    inline void write(const char* message) override
    {
        printf("[Assimp Internal] %s", message);
    }
};