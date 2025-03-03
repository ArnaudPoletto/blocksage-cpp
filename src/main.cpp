#define WIN32_LEAN_AND_MEAN
#include <GLFW/glfw3.h>
#include <windows.h>
#include <GL/glu.h>
#include <math.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include "window.h"
#include "renderer.h"
#include "region_reader.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

int main(void)
{
    // Window window(WINDOW_WIDTH, WINDOW_HEIGHT, "Blocksage");
    // if (!window.initialize())
    // {
    //     return -1;
    // }

    // Renderer renderer;
    // renderer.setCircleProperties(0.2f, 64);
    // renderer.startRenderLoop(window);
    // glfwTerminate();

    // return 0;

    // ================================================

    // Get region file path
    fs::path globalDir = fs::current_path().parent_path();
    fs::path regionFilePath = globalDir / "data" / "r.0.0.mca";

    // Get block id dictionary
    fs::path blockIdDictFilePath = globalDir / "data" / "block_id_dictionary.json";
    std::ifstream blockIdDictFile(blockIdDictFilePath);
    if (!blockIdDictFile.is_open())
    {
        throw std::runtime_error("Failed to open block id dictionary file: " + blockIdDictFilePath.string());
    }
    json blockIdDictJson = json::parse(blockIdDictFile);
    std::unordered_map<std::string, int> blockIdDict;
    for (auto it = blockIdDictJson.begin(); it != blockIdDictJson.end(); ++it)
    {
        blockIdDict[it.key()] = it.value();
    }

    RegionReader region_reader = RegionReader();
    Region region = region_reader.getRegion(regionFilePath, blockIdDict);

    return 0;
}