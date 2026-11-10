#pragma once
#include <array>
#include <vector>
#include <cmath>
#include "Mesh.h"
#include <glm/gtc/matrix_transform.hpp>
#include <map>
#include <tuple>
#include <limits>

namespace WorldGeometry
{
    enum class Shape { Box, Cylinder, Cone, Rock };
    struct Block {
        glm::vec3 center, size, color;
        Shape shape = Shape::Box;
        glm::vec3 rotation{0}; // Euler angles in degrees; visual geometry only.
    };
    struct Batch {
        Mesh mesh;
        glm::vec3 color{1}, low{0}, high{0};
    };

    std::vector<Vertex> Primitive(Shape shape);
    void Bake(const std::vector<Block>& blocks, std::vector<Batch>& batches);
}
