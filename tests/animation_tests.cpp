#include "../Project_Tarkov/Project_Tarkov/SkeletalAnimation.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

static void Require(bool result, const SkeletalAnimation& animation)
{
    if (!result) throw std::runtime_error(animation.Error());
}

static double Difference(const std::vector<AnimatedVertex>& a, const std::vector<AnimatedVertex>& b)
{
    assert(a.size() == b.size());
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i)
        sum += (a[i].position - b[i].position).SquareLength();
    return sum / a.size();
}

int main()
{
    try
    {
        SkeletalAnimation animation;
        Require(animation.LoadModel("Assets/Models/Player/Ch22_nonPBR.fbx"), animation);
        std::cout << "Bones: " << animation.BoneCount() << ", triangle vertices: " << animation.Vertices().size() << "\n";
        Require(animation.LoadClip("walk", "Assets/Walking.fbx"), animation);
        Require(animation.LoadClip("run", "Assets/Fast Run.fbx"), animation);
        Require(animation.LoadClip("crouch", "Assets/Crouched Walking.fbx"), animation);
        Require(animation.LoadClip("idle", "Assets/Idle.fbx"), animation);
        for (const auto* name : { "walk", "run", "crouch", "idle" })
        {
            animation.Update(0.2f, name, true);
            const auto first = animation.Vertices();
            animation.Update(0.3f, name, true);
            const double difference = Difference(first, animation.Vertices());
            std::cout << name << " deformation: " << difference << "\n";
            assert(difference > 1e-8);
            for (const auto& vertex : animation.Vertices())
            {
                assert(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) && std::isfinite(vertex.position.z));
                assert(vertex.position.SquareLength() < 10000000);
                assert(std::abs(vertex.normal.SquareLength() - 1) < 0.01f);
            }
        }
        // Generated idle must return to the same pose after exactly one 3.5 second cycle.
        animation.Update(0.2f, "idle", true);
        const auto first = animation.Vertices();
        for (int i = 0; i < 7; ++i) animation.Update(0.5f, "idle", true);
        assert(Difference(first, animation.Vertices()) < 0.001);
        const auto beforeError = animation.Vertices();
        assert(!animation.LoadClip("missing", "Assets/does-not-exist.fbx"));
        animation.Update(0.1f, "missing", true);
        assert(Difference(beforeError, animation.Vertices()) == 0);
        animation.Reset();
        assert(animation.Vertices().empty());
        std::cout << "Animation asset tests passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
