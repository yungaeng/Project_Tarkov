#include "CharacterVisual.h"
#include "../Game/Character.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Texture.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <array>
#include <vector>

namespace
{
    void AddTriangle(std::vector<Vertex>& vertices, const glm::vec3& a,
        const glm::vec3& b, const glm::vec3& c)
    {
        const auto normal = glm::normalize(glm::cross(b - a, c - a));
        for (const auto& position : {a, b, c}) {
            Vertex vertex{};
            vertex.pos = position;
            vertex.normal = normal;
            vertices.push_back(vertex);
        }
    }

    void AddQuad(std::vector<Vertex>& vertices, const glm::vec3& a,
        const glm::vec3& b, const glm::vec3& c, const glm::vec3& d)
    {
        AddTriangle(vertices, a, b, c);
        AddTriangle(vertices, a, c, d);
    }

    void CreateSportsBra(Mesh& mesh)
    {
        std::vector<Vertex> vertices;
        const float front = 20.f;
        AddQuad(vertices, {-19, 126, front}, {19, 126, front},
            {19, 132, front}, {-19, 132, front});
        AddQuad(vertices, {-19, 131, front}, {-2, 131, front},
            {-4, 146, front}, {-15, 146, front});
        AddQuad(vertices, {2, 131, front}, {19, 131, front},
            {15, 146, front}, {4, 146, front});
        AddQuad(vertices, {-17, 140, front}, {-13, 140, front},
            {-9, 164, front}, {-13, 164, front});
        AddQuad(vertices, {13, 140, front}, {17, 140, front},
            {13, 164, front}, {9, 164, front});
        mesh.Create(vertices);
    }

    void CreateSportsBraTrim(Mesh& mesh)
    {
        std::vector<Vertex> vertices;
        const float front = 20.6f;
        AddQuad(vertices, {-19, 126, front}, {19, 126, front},
            {19, 129, front}, {-19, 129, front});
        mesh.Create(vertices);
    }

    void CreateThong(Mesh& mesh)
    {
        std::vector<Vertex> vertices;
        const float front = 13.f;
        const float back = -13.f;
        AddQuad(vertices, {-18, 95, front}, {18, 95, front},
            {18, 100, front}, {-18, 100, front});
        AddQuad(vertices, {-18, 95, front}, {18, 95, front},
            {7, 78, front}, {-7, 78, front});
        AddQuad(vertices, {-18, 95, front}, {-14, 94, front},
            {-9, 91, front}, {-7, 78, front});
        AddQuad(vertices, {18, 95, front}, {14, 94, front},
            {9, 91, front}, {7, 78, front});
        AddQuad(vertices, {-18, 95, back}, {18, 95, back},
            {18, 100, back}, {-18, 100, back});
        AddQuad(vertices, {-18, 95, back}, {-14, 94, front},
            {-14, 94, back}, {-18, 95, front});
        AddQuad(vertices, {18, 95, back}, {14, 94, back},
            {14, 94, front}, {18, 95, front});
        AddQuad(vertices, {-3, 95, back}, {3, 95, back},
            {3, 80, back}, {-3, 80, back});
        mesh.Create(vertices);
    }

    void CreateThongTrim(Mesh& mesh)
    {
        std::vector<Vertex> vertices;
        const float front = 13.6f;
        const float back = -13.6f;
        AddQuad(vertices, {-18, 97, front}, {18, 97, front},
            {18, 99.5f, front}, {-18, 99.5f, front});
        AddQuad(vertices, {-18, 97, back}, {-18, 99.5f, back},
            {18, 99.5f, back}, {18, 97, back});
        mesh.Create(vertices);
    }

    void CreateBackpackCamouflage(std::array<Mesh, 4>& meshes)
    {
        std::array<std::vector<Vertex>, 4> patches;
        constexpr int columns = 16;
        constexpr int rows = 20;
        constexpr float left = -0.185f;
        constexpr float top = 1.385f;
        constexpr float cellWidth = 0.023f;
        constexpr float cellHeight = 0.025f;
        for (int row = 0; row < rows; ++row) {
            for (int column = 0; column < columns; ++column) {
                const unsigned pattern = static_cast<unsigned>((row * 37 + column * 19 + row * column * 11) % 17);
                if (pattern < 4) continue;
                const auto shade = static_cast<std::size_t>((row * 3 + column * 5 + pattern) % patches.size());
                const float x = left + column * cellWidth;
                const float y = top - row * cellHeight;
                const float width = cellWidth * (pattern % 3 == 0 ? 0.72f : 0.95f);
                const float height = cellHeight * (pattern % 4 == 0 ? 0.68f : 0.92f);
                const float z = -0.410f;
                AddQuad(patches[shade], {x, y - height, z}, {x, y, z},
                    {x + width, y, z}, {x + width, y - height, z});
            }
        }
        for (std::size_t shade = 0; shade < meshes.size(); ++shade)
            meshes[shade].Create(patches[shade]);
    }
}

struct CharacterVisual::Assets
{
    Mesh mesh, backpack, sportsBra, sportsBraTrim, thong, thongTrim;
    std::array<Mesh, 4> backpackCamouflage;
    Texture texture;
    MotionClip deathClip, holdClip, reloadClip, aimClip, recoilClip;
    std::array<MotionClip, 5> stances;
};

CharacterVisual::~CharacterVisual() { Reset(); }

void CharacterVisual::Init()
{
    Reset();
    if (!animation.LoadModel("Assets/Models/Player/Ch22_nonPBR.fbx") ||
        !animation.LoadAppearance("Assets/Models/Player/KimGawon/character.mesh") ||
        !animation.LoadClip("idle", "Assets/Idle.fbx") ||
        !animation.LoadClip("walk", "Assets/Walking.fbx") ||
        !animation.LoadClip("run", "Assets/Fast Run.fbx") ||
        !animation.LoadClip("crouch", "Assets/Crouched Walking.fbx"))
        throw std::runtime_error(animation.Error());
    static std::weak_ptr<Assets> cache;
    assets = cache.lock();
    if (!assets)
    {
        auto shared = std::make_shared<Assets>();
        if (!shared->texture.Load("Assets/Models/Player/KimGawon/character.png"))
            throw std::runtime_error("Character texture load failed");
        shared->backpack.CreateCube();
        CreateSportsBra(shared->sportsBra);
        CreateSportsBraTrim(shared->sportsBraTrim);
        CreateThong(shared->thong);
        CreateThongTrim(shared->thongTrim);
        CreateBackpackCamouflage(shared->backpackCamouflage);
        shared->deathClip.Load("Assets/Animations/Characters/Death.anim");
        shared->holdClip.Load("Assets/Animations/Characters/RifleHold.anim");
        shared->reloadClip.Load("Assets/Animations/Characters/RifleReload.anim");
        const char* names[] = {"RifleIdle", "RifleWalk", "RifleRun", "RifleCrouchIdle", "RifleCrouchWalk"};
        for (int i = 0; i < 5; ++i)
            shared->stances[i].Load(std::string("Assets/Animations/Characters/") + names[i] + ".anim");
        shared->aimClip.Load("Assets/Animations/Characters/RifleAim.anim");
        shared->recoilClip.Load("Assets/Animations/Characters/RifleRecoil.anim");
        animation.CreateMesh(shared->mesh);
        assets = shared;
        cache = shared;
    }
    weapon.Init();
    animation.Update(0.15f, "idle", true, false);
    animation.RefreshPose();
    GLint limit = 0;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &limit);
    if (animation.Palette().size() * 4 > static_cast<size_t>(limit))
        throw std::runtime_error("Bone palette exceeds GPU capacity");
    glGenBuffers(1, &paletteBuffer);
    glGenTextures(1, &paletteTexture);
    UploadPalette();
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_BUFFER, paletteTexture);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, paletteBuffer);
    glActiveTexture(GL_TEXTURE0);
}

void CharacterVisual::UploadPalette()
{
    const auto& palette = animation.Palette();
    glBindBuffer(GL_TEXTURE_BUFFER, paletteBuffer);
    // Orphan the small palette buffer to avoid waiting on the preceding draw.
    glBufferData(GL_TEXTURE_BUFFER, palette.size() * sizeof(glm::mat4), nullptr, GL_STREAM_DRAW);
    glBufferSubData(GL_TEXTURE_BUFFER, 0, palette.size() * sizeof(glm::mat4), palette.data());
}

void CharacterVisual::Update(float dt, const Character& character, const CharacterVisualState& next, float poseInterval)
{
    if (!assets) return;
    state = next;
    state.equipped = state.equipped && state.hasWeapon;
    recoilTime = state.fired ? 0 : recoilTime + dt;
    firePending |= state.fired;
    if (state.dead)
    {
        weapon.Update(dt, state.equipped, false, 0, false, false, true);
        if (!dying) { animation.CapturePose(); dying = true; deathTime = 0; }
        else if (deathTime >= assets->deathClip.Duration()) return; // Preserve the corpse, no loop or idle breathing.
        deathTime = (std::min)(assets->deathClip.Duration(), deathTime + dt);
        const auto previousHand = animation.NodePosition("RightHand");
        animation.ApplyMotion(assets->deathClip, deathTime, 1, false, true);
        weaponModel[3] += glm::vec4((animation.NodePosition("RightHand") - previousHand) *
            character.scale * character.GetSettings().modelScale, 0);
        deathRoot = assets->deathClip.Sample("Root", deathTime);
        // Keep the rotated mesh above the feet/support plane, including crouched deaths.
        const auto rotation = MotionPose{glm::vec3(0), deathRoot.rotation}.Matrix();
        float minimum = 0;
        for (const auto& vertex : animation.Vertices())
        {
            const glm::vec3 position(vertex.position.x, vertex.position.y, vertex.position.z);
            minimum = (std::min)(minimum, (rotation * glm::vec4(position * character.scale * character.GetSettings().modelScale, 1)).y);
        }
        deathRoot.position.y = (std::max)(deathRoot.position.y, -minimum + 0.01f);
        UploadPalette();
        return;
    }
    poseElapsed += dt;
    if (poseElapsed < poseInterval) return;
    dt = poseElapsed;
    poseElapsed = 0;
    weapon.Update(dt, state.equipped, state.aiming, state.reloadRemaining, firePending, character.IsMoving(), false, character.IsSprinting() && character.IsMoving() && !state.aiming, character.IsCrouching());
    firePending = false;
    const bool moving = character.IsMoving();
    const bool crouching = character.IsCrouching();
    const char* clip = crouching ? "crouch" : moving ? (character.IsSprinting() ? "run" : "walk") : "idle";
    animation.Update(dt, clip, moving || !crouching, false);
    const float blend = 1 - std::exp(-dt * 12);
    holdWeight += ((state.equipped ? 1.0f : 0.0f) - holdWeight) * blend;
    const bool sprint = moving && character.IsSprinting() && !crouching && !state.aiming;
    const int stance = crouching ? (moving ? 4 : 3) : moving ? (sprint ? 2 : 1) : 0;
    motionClock = std::fmod(motionClock + dt, 924.0f);
    aimWeight += ((state.aiming && !sprint && state.reloadRemaining <= 0 ? 1.f : 0.f) - aimWeight) * blend;
    for (int i = 0; i < 5; ++i) stanceWeights[i] += ((i == stance ? 1.f : 0.f) - stanceWeights[i]) * blend;
    if (holdWeight > 0.001f) animation.ApplyMotion(assets->holdClip, 0, holdWeight, true, false, false);
    for (int i = 0; i < 5; ++i) {
        const auto& motion = assets->stances[i];
        animation.ApplyMotion(motion, std::fmod(motionClock, motion.Duration()),
            stanceWeights[i] * holdWeight * (1 - aimWeight * .7f), false, false, false);
    }
    animation.ApplyMotion(assets->aimClip, 0, aimWeight * holdWeight, false, false, false);
    animation.ApplyMotion(assets->recoilClip, recoilTime, holdWeight, false, false, false);
    if (state.reloadRemaining > 0) animation.ApplyMotion(assets->reloadClip, 2 - state.reloadRemaining, holdWeight, false, false, false);
    animation.RefreshPose();
    if (state.hasWeapon && holdWeight > 0.001f) {
        const glm::vec3 scale = character.scale * character.GetSettings().modelScale;
        // RightArm is the shoulder joint. Anchor the butt pad ahead of the torso,
        // not behind a freely animated right hand.
        const glm::vec3 shoulder = animation.NodePosition("RightArm") * scale + glm::vec3(0.035f, -0.035f, 0.08f);
        auto grip = glm::translate(glm::mat4(1), shoulder);
        grip = glm::rotate(grip, glm::radians(-std::clamp(state.aimPitch, -89.f, 89.f)), glm::vec3(1, 0, 0));
        weaponModel = weapon.ModelMatrix(grip);
        const auto right = glm::vec3(weaponModel * glm::vec4(WeaponVisual::FiringGrip(), 1)) / scale;
        const auto left = glm::vec3(weaponModel * glm::vec4(weapon.SupportGrip(), 1)) / scale;
        const glm::vec3 rightPalm = glm::vec3(weaponModel * glm::vec4(1, 0, 0.4f, 0)) / scale;
        const glm::vec3 leftPalm = glm::vec3(weaponModel * glm::vec4(-0.3f, 0, 1, 0)) / scale;
        animation.SolveArm("Right", right, (shoulder + glm::vec3(-0.3f, -0.4f, 0.1f)) / scale, rightPalm, holdWeight);
        animation.SolveArm("Left", left, (shoulder + glm::vec3(0.5f, -0.4f, 0.1f)) / scale, leftPalm, holdWeight);
        animation.RefreshPose();
    }
    UploadPalette();
}

void CharacterVisual::Render(const Character& character, Shader& shader, Camera& camera, float width, float height, const glm::vec3& color)
{
    if (!assets) return;
    glm::mat4 model = glm::translate(glm::mat4(1), character.position);
    model = glm::rotate(model, glm::radians(character.rotation.y), glm::vec3(0, 1, 0));
    if (dying) model *= deathRoot.Matrix();
    const glm::mat4 actor = model;
    model = glm::scale(model, character.scale * character.GetSettings().modelScale);
    // Bone envelopes conservatively enclose the current skinned pose.
    // Padding also covers the attached rifle, magazine and muzzle flash.
    const float weaponPadding = 2.0f;
    if (!Renderer::IsVisible(animation.BoundsMin(), animation.BoundsMax(), model, weaponPadding)) return;
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_BUFFER, paletteTexture);
    glActiveTexture(GL_TEXTURE0);
    Renderer::Draw(shader, assets->mesh, camera, model, width, height, glm::vec3(dying ? 0.65f : 1.f), true, assets->texture.id, static_cast<int>(character.clothingMask));
    const auto equipped = character.clothingMask;
    const auto scale = character.scale * character.GetSettings().modelScale;
    const auto garmentModel = glm::scale(actor, scale);
    if ((equipped & ClothingBit(ItemType::UnderShirt)) &&
        !(equipped & ClothingBit(ItemType::Shirt))) {
        Renderer::Draw(shader, assets->sportsBra, camera, garmentModel, width, height, glm::vec3(.035f,.035f,.04f));
        Renderer::Draw(shader, assets->sportsBraTrim, camera, garmentModel, width, height, glm::vec3(.42f,.42f,.39f));
    }
    if ((equipped & ClothingBit(ItemType::Underpants)) &&
        !(equipped & ClothingBit(ItemType::Trousers))) {
        Renderer::Draw(shader, assets->thong, camera, garmentModel, width, height, glm::vec3(.025f,.025f,.03f));
        Renderer::Draw(shader, assets->thongTrim, camera, garmentModel, width, height, glm::vec3(.48f,.48f,.45f));
    }
    if (character.clothingMask & ClothingBit(ItemType::Backpack)) {
        auto cubePart = [&](const glm::vec3& position, const glm::vec3& size, const glm::vec3& color) {
            auto part = glm::translate(actor, position);
            part = glm::scale(part, size);
            Renderer::Draw(shader, assets->backpack, camera, part, width, height, color);
        };
        auto pack = glm::translate(actor, glm::vec3(0, 1.13f, -0.25f));
        pack = glm::scale(pack, glm::vec3(.40f,.52f,.22f));
        Renderer::Draw(shader, assets->backpack, camera, pack, width, height, glm::vec3(.20f,.24f,.18f));
        cubePart({0,1.01f,-.375f},{.31f,.24f,.065f},{.24f,.27f,.20f});
        cubePart({0,1.30f,-.36f},{.28f,.12f,.055f},{.27f,.29f,.22f});
        for (float side : {-.14f,.14f})
            cubePart({side,1.13f,-.38f},{.045f,.53f,.035f},{.30f,.32f,.24f});

        const glm::vec3 camoColors[]{
            {.30f,.32f,.23f}, {.16f,.19f,.14f}, {.38f,.35f,.27f}, {.10f,.13f,.10f}
        };
        for (std::size_t shade = 0; shade < assets->backpackCamouflage.size(); ++shade)
            Renderer::Draw(shader,assets->backpackCamouflage[shade],camera,actor,width,height,camoColors[shade]);
        for (float row = .91f; row <= 1.27f; row += .075f) {
            cubePart({0,row,-.422f},{.31f,.018f,.014f},{.19f,.22f,.17f});
            for (float side : {-.105f,.105f})
                cubePart({side,row,-.433f},{.035f,.027f,.016f},{.36f,.35f,.29f});
        }
    }
    if (state.hasWeapon) weapon.Render(actor * weaponModel, shader, camera, width, height);
}

void CharacterVisual::Reset()
{
    if (paletteTexture) glDeleteTextures(1, &paletteTexture);
    if (paletteBuffer) glDeleteBuffers(1, &paletteBuffer);
    paletteTexture = paletteBuffer = 0;
    assets.reset();
    poseElapsed = 0;
    animation.Reset();
    weapon.Reset();
    weaponModel = glm::mat4(1);
    dying = false;
    firePending = false;
    deathTime = holdWeight = motionClock = aimWeight = 0;
    recoilTime = 1;
    stanceWeights = {};
    deathRoot = {};
    state = {};
}
