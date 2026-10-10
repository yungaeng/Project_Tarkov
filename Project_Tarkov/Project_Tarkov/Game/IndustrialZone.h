#pragma once
#include "../Physics/CollisionWorld.h"
#include "Config/RaidConfig.h"
#include <array>
#include <vector>

// North is -Z. All positions use meters and the ground surface is Y=0.
namespace IndustrialZone
{
    enum class Shape { Box, Cylinder, Cone, Rock };
    struct Block {
        glm::vec3 center, size, color;
        Shape shape = Shape::Box;
        glm::vec3 rotation{0}; // Euler angles in degrees; visual geometry only.
    };
    struct Exit { const char* name; glm::vec3 position; };
    inline const std::array<glm::vec3, 2> Spawns{{ {-210, 0, 210}, {-210, 0, -210} }};
    inline const std::array<Exit, 2> Exits{{
        {"X1 북동쪽 검문소", {225, 0, -220}}, {"X2 남동쪽 폐도로", {225, 0, 220}}
    }};
    inline const std::array<glm::vec3, 5> Buildings{{
        {-125, 0, -105}, {-125, 0, -35}, {0, 0, 0}, {-120, 0, 110}, {120, 0, 110}
    }};
    inline const std::array<glm::vec3, 5> Enemies{{
        {-125, 0, -105}, {111, 0, -105}, {0, 0, 0}, {-120, 0, 110}, {120, 0, 110}
    }};

    inline void Build(CollisionWorld& world, std::vector<Block>& blocks)
    {
        world.Clear(); blocks.clear();
        auto box = [&](glm::vec3 p, glm::vec3 s, glm::vec3 c, bool solid = true) {
            blocks.push_back({p, s, c});
            if (solid) world.AddBox({p - s * 0.5f, p + s * 0.5f});
        };
        const glm::vec3 concrete(0.43f, 0.45f, 0.43f), road(0.19f, 0.20f, 0.21f);
        box({0, -0.5f, 0}, {RaidConfig::MapWidth, 1, RaidConfig::MapLength}, {0.27f, 0.31f, 0.24f});
        for (float side : {-1.0f, 1.0f}) {
            box({side * 250.5f, 3, 0}, {1, 6, 502}, concrete);
            box({0, 3, side * 250.5f}, {500, 6, 1}, concrete);
            box({side * 210, 0.015f, 0}, {12, 0.02f, 480}, road, false);
            box({0, 0.015f, side * 210}, {432, 0.02f, 12}, road, false);
        }
        box({0, 0.015f, 70}, {420, 0.02f, 12}, road, false);
        box({70, 0.015f, 0}, {12, 0.02f, 420}, road, false);
        // Five walk-in shells. Opposite 6m entrances provide two routes per building.
        for (std::size_t i = 0; i < Buildings.size(); ++i) {
            const auto p = Buildings[i];
            const float w = i == 2 ? 64.0f : 40.0f, d = i == 2 ? 54.0f : 30.0f;
            const float h = i == 2 ? 10.0f : 6.0f;
            const glm::vec3 color = i == 2 ? glm::vec3(0.50f, 0.36f, 0.28f) : concrete;
            for (float side : {-1.0f, 1.0f}) {
                box(p + glm::vec3(side * w / 2, h / 2, 0), {1, h, d}, color);
                for (float wing : {-1.0f, 1.0f})
                    box(p + glm::vec3(wing * (w + 6) / 4, h / 2, side * d / 2), {(w - 6) / 2, h, 1}, color);
                box(p + glm::vec3(0, (h + 3) / 2, side * d / 2), {6, h - 3, 1}, color);
            }
            box(p + glm::vec3(0, h + 0.25f, 0), {w + 1, 0.5f, d + 1}, color);
        }
        // Container yard B: 40 containers with 6m wide lanes.
        for (int row = 0; row < 5; ++row)
            for (int col = 0; col < 8; ++col)
                box({78.0f + col * 10, 1.5f, -155.0f + row * 18}, {4, 3, 12},
                    row % 2 ? glm::vec3(0.25f, 0.38f, 0.44f) : glm::vec3(0.52f, 0.29f, 0.20f));
        // Peripheral tree trunks and low cover leave the ring road clear.
        for (int i = 0; i < 18; ++i) {
            const float a = -190.0f + i * 22;
            box({a, 0.7f, 190}, {4, 1.4f, 2}, concrete);
            box({a, 0.7f, -190}, {4, 1.4f, 2}, concrete);
        }
        for (auto p : Spawns) box(p + glm::vec3(8, 1, 0), {4, 2, 3}, concrete);
        for (const auto& exit : Exits) {
            box(exit.position + glm::vec3(0, 0.035f, 0), {12, 0.04f, 12}, {0.15f, 0.65f, 0.28f}, false);
            box(exit.position + glm::vec3(8, 3, 0), {1, 6, 1}, {0.85f, 0.75f, 0.22f});
        }
    }
}
