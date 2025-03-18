#include <ft2build.h>
#include FT_FREETYPE_H
#include "opengl_headers.h"
#include <windows.h>
#include <GL/glu.h>
#include <math.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include "window.h"
#include "renderer/renderer.h"
#include "region_reader.h"

namespace fs = std::filesystem;
using json = nlohmann::json;

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

int main(void)
{
    // Get file paths
    fs::path globalDir = fs::current_path().parent_path();
    fs::path regionFilePath = globalDir / "data" / "r.0.0_2.mca";
    fs::path blockIdDictFilePath = globalDir / "data" / "block_id_dictionary.json";
    fs::path blockColorDictFilePath = globalDir / "data" / "block_color_dictionary.json";

    // Get block id dictionary
    std::ifstream blockIdDictFile(blockIdDictFilePath);
    if (!blockIdDictFile.is_open())
    {
        throw std::runtime_error("Failed to open block id dictionary file: " + blockIdDictFilePath.string());
    }
    json blockIdDictJson = json::parse(blockIdDictFile);
    std::unordered_map<std::string, uint16_t> blockIdDict;
    for (auto it = blockIdDictJson.begin(); it != blockIdDictJson.end(); ++it)
    {
        blockIdDict[it.key()] = static_cast<uint16_t>(it.value());
    }

    // Get block color dictionary
    std::ifstream blockColorDictFile(blockColorDictFilePath);
    if (!blockColorDictFile.is_open())
    {
        throw std::runtime_error("Failed to open block color dictionary file: " + blockColorDictFilePath.string());
    }
    json blockColorDictJson = json::parse(blockColorDictFile);
    std::unordered_map<uint16_t, glm::vec3> blockColorDict;
    for (auto it = blockColorDictJson.begin(); it != blockColorDictJson.end(); ++it)
    {
        int blockId = blockIdDict[it.key()];
        blockColorDict[blockId] = glm::vec3(it.value()[0], it.value()[1], it.value()[2]);
    }

    // Get no render block id list
    std::vector<uint16_t> noRenderBlockIds = {};
    noRenderBlockIds.push_back(blockIdDict["air"]);

    // Get region
    RegionReader region_reader = RegionReader();
    Region region = region_reader.getRegion(regionFilePath, blockIdDict);

    // Initialize window
    Window window(WINDOW_WIDTH, WINDOW_HEIGHT, "Blocksage");
    if (!window.initialize())
    {
        std::cerr << "Failed to initialize window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Initialize renderer
    Renderer renderer(blockColorDict, noRenderBlockIds);
    if (!renderer.initialize())
    {
        std::cerr << "Failed to initialize renderer" << std::endl;
        glfwTerminate();
        return -1;
    }
    renderer.setRegion(&region);
    renderer.startRenderLoop(window);

    window.cleanup();
    glfwTerminate();
    std::cout << "Exiting..." << std::endl;

    return 0;
}