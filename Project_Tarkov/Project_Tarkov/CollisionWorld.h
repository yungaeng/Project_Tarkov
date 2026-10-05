#pragma once
#include <glm/vec3.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

struct StaticBox
{
    glm::vec3 min;
    glm::vec3 max;
};

// Axis-aligned static geometry. Character positions are at the centre of their feet.
class CollisionWorld
{
private:
    std::vector<StaticBox> boxes;
    static float Component(const glm::vec3& v, int axis)
    {
        return axis == 0 ? v.x : axis == 1 ? v.y : v.z;
    }
    static void Translate(glm::vec3& v, int axis, float amount)
    {
        if (axis == 0) v.x += amount;
        else if (axis == 1) v.y += amount;
        else v.z += amount;
    }
    static StaticBox Bounds(const glm::vec3& feet, float radius, float height)
    {
        return { { feet.x - radius, feet.y, feet.z - radius },
                 { feet.x + radius, feet.y + height, feet.z + radius } };
    }
    static bool Overlaps(const StaticBox& a, const StaticBox& b, int axis)
    {
        return Component(a.max, axis) > Component(b.min, axis) &&
               Component(a.min, axis) < Component(b.max, axis);
    }

public:
    void AddBox(const StaticBox& box)
    {
        for (int axis = 0; axis < 3; ++axis)
            if (!std::isfinite(Component(box.min, axis)) ||
                !std::isfinite(Component(box.max, axis)) ||
                Component(box.min, axis) >= Component(box.max, axis))
                return;
        boxes.push_back(box);
    }
    void Clear() { boxes.clear(); }
    const std::vector<StaticBox>& GetBoxes() const { return boxes; }

    // Sweep the full distance on one axis, so thin walls cannot be skipped.
    bool MoveAxis(glm::vec3& feet, float radius, float height, int axis, float distance) const
    {
        const StaticBox body = Bounds(feet, radius, height);
        float allowed = distance;
        for (const auto& box : boxes)
        {
            if (!Overlaps(body, box, (axis + 1) % 3) ||
                !Overlaps(body, box, (axis + 2) % 3))
                continue;
            if (distance > 0 && Component(body.max, axis) <= Component(box.min, axis))
                allowed = (std::min)(allowed, Component(box.min, axis) - Component(body.max, axis));
            if (distance < 0 && Component(body.min, axis) >= Component(box.max, axis))
                allowed = (std::max)(allowed, Component(box.max, axis) - Component(body.min, axis));
        }
        Translate(feet, axis, allowed);
        return allowed != distance;
    }

    bool IsSupported(const glm::vec3& feet, float radius, float height) const
    {
        const auto body = Bounds(feet, radius, height);
        for (const auto& box : boxes)
            if (Overlaps(body, box, 0) && Overlaps(body, box, 2) &&
                std::abs(feet.y - box.max.y) <= 0.0001f)
                return true;
        return false;
    }

    // Recover small spawn/teleport overlaps. Invalid enclosed spawns should be avoided.
    void ResolveOverlap(glm::vec3& feet, float radius, float height) const
    {
        for (int pass = 0; pass < 8; ++pass)
        {
            bool resolved = false;
            for (const auto& box : boxes)
            {
                const auto body = Bounds(feet, radius, height);
                if (!Overlaps(body, box, 0) || !Overlaps(body, box, 1) || !Overlaps(body, box, 2))
                    continue;
                float smallest = 1.0e30f;
                int bestAxis = 1;
                for (int axis = 0; axis < 3; ++axis)
                {
                    const float negative = Component(box.min, axis) - Component(body.max, axis);
                    const float positive = Component(box.max, axis) - Component(body.min, axis);
                    const float push = std::abs(negative) < std::abs(positive) ? negative : positive;
                    if (std::abs(push) < std::abs(smallest)) { smallest = push; bestAxis = axis; }
                }
                Translate(feet, bestAxis, smallest);
                resolved = true;
            }
            if (!resolved) break;
        }
    }
};
