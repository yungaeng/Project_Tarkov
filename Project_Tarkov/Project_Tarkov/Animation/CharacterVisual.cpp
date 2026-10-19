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
#include <map>

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

    void CreateBackpackShell(Mesh& mesh)
    {
        // Rounded rectangular cross sections: tapered base and padded top.
        std::vector<Vertex> vertices;
        constexpr int segments = 24;
        const float heights[] = {-.28f, -.25f, -.19f, .18f, .23f, .25f};
        const float widths[] = {.12f, .17f, .19f, .18f, .16f, .11f};
        const float depths[] = {.065f, .105f, .12f, .115f, .09f, .055f};
        auto point = [&](int ring, int step) {
            const float angle = step * 6.2831853f / segments;
            const float c = std::cos(angle), sn = std::sin(angle);
            return glm::vec3(widths[ring] * std::copysign(std::pow(std::abs(c), .5f), c),
                heights[ring], depths[ring] * std::copysign(std::pow(std::abs(sn), .5f), sn));
        };
        for (int ring = 0; ring < 5; ++ring)
            for (int i = 0; i < segments; ++i)
                AddQuad(vertices, point(ring,i), point(ring+1,i), point(ring+1,i+1), point(ring,i+1));
        for (int i = 0; i < segments; ++i) {
            AddTriangle(vertices, {0,heights[0],0}, point(0,i), point(0,i+1));
            AddTriangle(vertices, {0,heights[5],0}, point(5,i+1), point(5,i));
        }
        mesh.Create(vertices);
    }

}

struct CharacterVisual::Assets
{
    Mesh mesh, backpack, backpackDetail;
    Texture texture;
    MotionClip deathClip, lootClip, lootSearchClip, holdClip, reloadClip, aimClip, recoilClip;
    MotionClip jumpClip, landClip;
    MotionClip proneClip, crawlClip;
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
        CreateBackpackShell(shared->backpack);
        shared->backpackDetail.CreateCube();
        shared->deathClip.Load("Assets/Animations/Characters/Death.anim");
        shared->jumpClip.Load("Assets/Animations/Characters/Jump.anim");
        shared->landClip.Load("Assets/Animations/Characters/Land.anim");
        shared->proneClip.Load("Assets/Animations/Characters/Prone.anim");
        shared->crawlClip.Load("Assets/Animations/Characters/ProneCrawl.anim");
        shared->lootClip.Load("Assets/Animations/Characters/CorpseLoot.anim");
        shared->lootSearchClip.Load("Assets/Animations/Characters/CorpseSearch.anim");
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
    // Smooth, bounded secondary motion; stop oscillation on death.
    const float packMoving = character.IsMoving() && !state.dead && !character.IsLooting() ? 1.f : 0.f;
    backpackMotion += (packMoving - backpackMotion) * (1.f - std::exp(-dt * 7.f));
    if (!state.dead) backpackClock = std::fmod(backpackClock + dt * (character.IsSprinting() ? 11.f : 7.f), 6.2831853f);

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
        animation.ApplyMotion(assets->deathClip, deathTime, std::clamp(deathTime / .65f, 0.f, 1.f), true, true);
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
    if (character.IsLooting()) {
        // Keep the knees planted while searching the waist pockets.
        AnimateLootSearch(dt);
        UploadPalette();
        return;
    }
    // Velocity drives the pose so a ceiling hit or a long fall cannot loop the jump.
    // Stationary preview characters have no vertical velocity and stay in idle.
    const bool airborne = !character.IsGrounded() &&
        (wasAirborne || std::abs(character.GetVerticalVelocity()) > .001f);
    if (wasAirborne && character.IsGrounded()) landingTime = 0;
    else landingTime += dt;
    wasAirborne = airborne;
    if (airborne) {
        jumpPhase = std::clamp(.5f - character.GetVerticalVelocity() /
            (2.f * character.GetSettings().jumpSpeed), 0.f, 1.f);
        landingTime = assets->landClip.Duration();
    }
    jumpWeight += ((airborne ? 1.f : 0.f) - jumpWeight) * (1.f - std::exp(-dt * 18.f));
    poseElapsed += dt;
    if (poseElapsed < poseInterval) return;
    dt = poseElapsed;
    poseElapsed = 0;
    weapon.Update(dt, state.equipped, state.aiming, state.reloadRemaining, firePending, character.IsMoving(), false, character.IsSprinting() && character.IsMoving() && !state.aiming, character.IsCrouching());
    firePending = false;
    const bool moving = character.IsMoving();
    const bool crouching = character.IsCrouching();
    const bool prone = character.IsProne();
    const char* clip = airborne || prone ? "idle" : crouching ? "crouch" : moving ? (character.IsSprinting() ? "run" : "walk") : "idle";
    const auto& movement = character.GetSettings();
    const float referenceSpeed = prone ? movement.proneSpeed : crouching ? movement.crouchSpeed :
        character.IsSprinting() ? movement.runSpeed : movement.walkSpeed;
    const float pace = moving ? std::clamp(character.GetMoveSpeed() / referenceSpeed, 0.f, 1.5f) : 1.f;
    animation.Update(dt, clip, moving && !prone && !airborne, false, pace);
    const float blend = 1 - std::exp(-dt * 12);
    proneWeight += ((prone ? 1.f : 0.f) - proneWeight) * (1.f - std::exp(-dt * 7.f));
    crawlWeight += ((prone && moving ? 1.f : 0.f) - crawlWeight) * blend;
    if (prone && moving) crawlClock = std::fmod(crawlClock + dt * pace, assets->crawlClip.Duration());
    holdWeight += ((state.equipped ? 1.0f : 0.0f) - holdWeight) * blend;
    const bool sprint = moving && character.IsSprinting() && !crouching && !state.aiming;
    const int stance = prone ? 0 : crouching ? (moving ? 4 : 3) : moving ? (sprint ? 2 : 1) : 0;
    motionClock = std::fmod(motionClock + dt, 924.0f);
    aimWeight += ((state.aiming && !sprint && state.reloadRemaining <= 0 ? 1.f : 0.f) - aimWeight) * blend;
    for (int i = 0; i < 5; ++i) stanceWeights[i] += ((i == stance ? 1.f : 0.f) - stanceWeights[i]) * blend;
    if (holdWeight > 0.001f) animation.ApplyMotion(assets->holdClip, 0, holdWeight, true, false, false);
    for (int i = 0; i < 5; ++i) {
        const auto& motion = assets->stances[i];
        const float phase = i == 1 || i == 2 || i == 4 ? animation.CyclePhase() * motion.Duration() :
            std::fmod(motionClock, motion.Duration());
        animation.ApplyMotion(motion, phase,
            stanceWeights[i] * holdWeight * (1.f - proneWeight) * (1.f - jumpWeight), true, false, false);
    }
    animation.ApplyMotion(assets->proneClip, 0, proneWeight, true, false, false);
    animation.ApplyMotion(assets->crawlClip, crawlClock, crawlWeight * proneWeight, false, false, false);
    animation.ApplyMotion(assets->aimClip, 0, aimWeight * holdWeight, false, false, false);
    animation.ApplyMotion(assets->recoilClip, recoilTime, holdWeight, false, false, false);
    if (state.reloadRemaining > 0) animation.ApplyMotion(assets->reloadClip, 2 - state.reloadRemaining, holdWeight, false, false, false);
    animation.ApplyMotion(assets->jumpClip, jumpPhase, jumpWeight * (1.f - proneWeight), true, false, false);
    if (!airborne && !prone && landingTime < assets->landClip.Duration())
        animation.ApplyMotion(assets->landClip, landingTime, 1.f, false, false, false);
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
        auto left = glm::vec3(weaponModel * glm::vec4(weapon.SupportGrip(), 1)) / scale;
        const glm::vec3 rightPalm = glm::vec3(weaponModel * glm::vec4(1, 0, 0.4f, 0)) / scale;
        glm::vec3 leftPalm = glm::vec3(weaponModel * glm::vec4(-0.3f, 0, 1, 0)) / scale;
        // Release the support hand during crawling; otherwise rifle IK erases the crawl.
        const float support = crawlWeight * proneWeight * (1.f - aimWeight) *
            (state.reloadRemaining > 0 || recoilTime < .15f ? 0.f : 1.f);
        const float phase = crawlClock / assets->crawlClip.Duration();
        const float reach = std::cos(phase * 6.2831853f);
        const float lift = (std::max)(0.f, std::sin(phase * 6.2831853f));
        left = glm::mix(left, glm::vec3(.25f, .12f + .07f * lift, .48f + .15f * reach) / scale, support);
        leftPalm = glm::mix(leftPalm, glm::vec3(0, -1, .15f) / scale, support);
        // Tuck the elbows during standing locomotion without changing the grip targets.
        const float compact = (stanceWeights[0] + stanceWeights[1] + stanceWeights[2]) *
            (1.f - proneWeight) * (state.reloadRemaining > 0 ? 0.f : 1.f);
        auto rightElbow = glm::mix(glm::vec3(-.3f, -.4f, .1f), glm::vec3(-.18f, -.36f, .08f), compact);
        auto leftElbow = glm::mix(glm::vec3(.5f, -.4f, .1f), glm::vec3(.30f, -.34f, .16f), compact);
        rightElbow = glm::mix(shoulder + rightElbow, glm::vec3(-.24f, .12f, .32f), proneWeight);
        leftElbow = glm::mix(shoulder + leftElbow, glm::vec3(.30f, .12f, .32f), proneWeight);
        animation.SolveArm("Right", right, rightElbow / scale, rightPalm, holdWeight);
        animation.SolveArm("Left", left, leftElbow / scale, leftPalm, holdWeight);
        animation.RefreshPose();
    }
    UploadPalette();
}

glm::vec3 CharacterVisual::FirstPersonCameraPosition(const Character& character) const
{
    const auto actor = glm::translate(glm::mat4(1), character.position) *
        glm::rotate(glm::mat4(1), glm::radians(character.rotation.y), glm::vec3(0, 1, 0));
    if (!assets) return glm::vec3(actor * glm::vec4(0, character.GetEyeHeight(), .4f, 1));
    const auto scale = character.scale * character.GetSettings().modelScale;
    const auto head = animation.NodePosition("Head") * scale;
    glm::vec3 camera = head + glm::vec3(0, .14f, .12f);
    // Conservative animated body spheres, expanded by the camera near-plane radius.
    // Solve toward the actor's front, independent of look pitch: looking down cannot
    // pull the camera backward into the chest. Helmet and carrier padding are included.
    const char* joints[] = {"Hips", "Spine", "Spine1", "Spine2", "Neck", "Head", "LeftArm", "RightArm"};
    const float radii[] = {.24f, .25f, .26f, .27f, .15f, .23f, .16f, .16f};
    constexpr float cameraRadius = .22f;
    for (int i = 0; i < 8; ++i) {
        const auto center = animation.NodePosition(joints[i]) * scale;
        const float radius = radii[i] + cameraRadius;
        const auto xy = glm::vec2(camera.x - center.x, camera.y - center.y);
        const float remaining = radius * radius - glm::dot(xy, xy);
        if (remaining > 0)
            camera.z = (std::max)(camera.z, center.z + std::sqrt(remaining) + .005f);
    }
    return glm::vec3(actor * glm::vec4(camera, 1));
}

void CharacterVisual::Render(const Character& character, Shader& shader, Camera& camera, float width, float height,
    const glm::vec3& color, bool firstPerson)
{
    if (!assets) return;
    glm::mat4 model = glm::translate(glm::mat4(1), character.position);
    model = glm::rotate(model, glm::radians(character.rotation.y), glm::vec3(0, 1, 0));
    if (dying) model *= deathRoot.Matrix();
    else if (character.IsLooting()) {
        auto root = assets->lootClip.Sample("Root", 0);
        model *= root.Matrix();
    }
    const glm::mat4 actor = model;
    model = glm::scale(model, character.scale * character.GetSettings().modelScale);
    // Bone envelopes conservatively enclose the current skinned pose.
    // Padding also covers the attached rifle, magazine and muzzle flash.
    const float weaponPadding = 2.0f;
    if (!Renderer::IsVisible(animation.BoundsMin(), animation.BoundsMax(), model, weaponPadding)) return;
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_BUFFER, paletteTexture);
    glActiveTexture(GL_TEXTURE0);
    Renderer::Draw(shader, assets->mesh, camera, model, width, height,
        glm::vec3(dying ? 0.65f : 1.f), true, assets->texture.id,
        static_cast<int>(character.clothingMask), 1.f, firstPerson);
    // Fitted underwear is part of the skinned appearance and uses its clothing mask.
    if (character.clothingMask & ClothingBit(ItemType::Backpack)) {
        // Build a torso frame from the posed skeleton, including crouch, search and death.
        const auto scale = character.scale * character.GetSettings().modelScale;
        const auto chest = animation.NodePosition("Spine2") * scale;
        auto up = glm::normalize(chest - animation.NodePosition("Hips") * scale);
        auto right = glm::normalize((animation.NodePosition("LeftArm") - animation.NodePosition("RightArm")) * scale);
        auto forward = glm::normalize(glm::cross(right, up));
        right = glm::normalize(glm::cross(up, forward));
        glm::mat4 frame(1);
        frame[0] = glm::vec4(right,0); frame[1] = glm::vec4(up,0); frame[2] = glm::vec4(forward,0);
        frame[3] = glm::vec4(chest,1);
        const auto harness = actor * frame;
        auto pack = glm::translate(harness, glm::vec3(0,-.08f,-.25f));
        const float amplitude = state.dead ? 0.f : backpackMotion * (character.IsSprinting() ? 1.f : .55f);
        pack = glm::translate(pack, glm::vec3(0, .008f * amplitude * std::sin(backpackClock*2), 0));
        pack = glm::rotate(pack, glm::radians(2.5f * amplitude * std::sin(backpackClock)), glm::vec3(0,0,1));
        pack = glm::rotate(pack, glm::radians(1.5f * amplitude * std::cos(backpackClock*2)), glm::vec3(1,0,0));
        Renderer::Draw(shader, assets->backpack, camera, pack, width, height, {.22f,.25f,.17f});
        auto part = [&](const glm::mat4& parent, glm::vec3 center, glm::vec3 size, glm::vec3 color) {
            Renderer::Draw(shader, assets->backpackDetail, camera,
                glm::scale(glm::translate(parent,center),size), width,height,color);
        };
        // Front admin pouch, lower utility pocket, side pockets, compression straps.
        part(pack,{0,.105f,-.13f},{.27f,.13f,.055f},{.26f,.29f,.20f});
        part(pack,{0,-.12f,-.145f},{.29f,.22f,.08f},{.20f,.23f,.15f});
        for (float side : {-1.f,1.f}) {
            part(pack,{side*.20f,-.10f,0},{.075f,.23f,.16f},{.22f,.25f,.17f});
            for (float y : {-.16f,.10f}) {
                part(pack,{side*.20f,y,-.015f},{.084f,.023f,.19f},{.13f,.16f,.10f});
                part(pack,{side*.20f,y,-.117f},{.04f,.032f,.018f},{.09f,.10f,.08f});
            }
            // Shoulder harness follows the torso, separately from the swaying bag.
            part(harness,{side*.105f,.015f,-.095f},{.045f,.25f,.035f},{.15f,.18f,.12f});
            part(harness,{side*.105f,.15f,-.025f},{.045f,.035f,.18f},{.15f,.18f,.12f});
        }
        for (float y : {-.19f,-.13f,-.07f})
            part(pack,{0,y,-.19f},{.27f,.018f,.012f},{.31f,.32f,.23f});
        part(pack,{0,.265f,0},{.12f,.025f,.035f},{.12f,.14f,.09f});
        for (float x : {-.065f,.065f})
            part(pack,{x,.245f,0},{.02f,.045f,.035f},{.12f,.14f,.09f});
        part(pack,{0,.18f,-.134f},{.26f,.008f,.012f},{.08f,.09f,.07f});
        part(pack,{.10f,.165f,-.15f},{.012f,.035f,.015f},{.40f,.41f,.34f});

    }
    if (state.hasWeapon && !character.IsLooting()) weapon.Render(actor * weaponModel, shader, camera, width, height);
}

glm::vec3 CharacterVisual::FitLootPose(const Character& character, const Character& corpse,
    const CharacterVisual& corpseVisual,
    const std::function<bool(const glm::vec3&, float, const std::array<glm::vec3, 4>&)>& placementClear)
{
    animation.Update(0, "idle", false, false);
    animation.ApplyMotion(assets->lootClip, 0, 1, true, false, true);
    lootPoseValid = false;
    auto corpseModel = glm::translate(glm::mat4(1), corpse.position);
    corpseModel = glm::rotate(corpseModel, glm::radians(corpse.rotation.y), glm::vec3(0, 1, 0));
    corpseModel *= corpseVisual.deathRoot.Matrix();
    corpseModel = glm::scale(corpseModel, corpse.scale * corpse.GetSettings().modelScale);
    auto lootModel = glm::rotate(glm::mat4(1), glm::radians(corpse.rotation.y + 180.f), glm::vec3(0, 1, 0));
    lootModel *= assets->lootClip.Sample("Root", 0).Matrix();
    lootModel = glm::scale(lootModel, character.scale * character.GetSettings().modelScale);
    const auto pelvis = glm::vec3(corpseModel * glm::vec4(corpseVisual.animation.NodePosition("Hips"), 1));
    // The living player kneels toward the supine enemy head.
    // Align the reach of the authored kneel, not the two pelvis joints.
    // Pelvis alignment put the looter directly on top of the corpse.
    const auto handCenter = (animation.NodePosition("RightHand") + animation.NodePosition("LeftHand")) * .5f;
    const auto localHands = glm::vec3(lootModel * glm::vec4(handCenter, 1));
    const glm::vec3 anchor(pelvis.x - localHands.x, corpse.position.y, pelvis.z - localHands.z);
    lootModel = glm::translate(glm::mat4(1), anchor) * lootModel;
    // Preserve the authored folded shins instead of rotating each thigh alone.

    // Build a padded surface envelope once on entry. Include clothing so that
    // fitting the pelvis also preserves room for the torso and raised thighs.
    constexpr float cell = .04f;
    constexpr float clearance = .02f;
    using Interval = std::pair<float, float>;
    std::map<std::pair<int, int>, Interval> surface;
    // Cover triangle interiors as well as vertices, including large clothing
    // triangles. Each cell stores a conservative vertical surface bound.
    auto visitSurface = [&](const SkeletalAnimation& mesh, const glm::mat4& model, auto visit) {
        const auto& vertices = mesh.Vertices();
        const auto& indices = mesh.Indices();
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            const auto point = [&](unsigned int index) {
                const auto& v = vertices[index].position;
                return glm::vec3(model * glm::vec4(v.x, v.y, v.z, 1));
            };
            const auto a = point(indices[i]);
            const auto b = point(indices[i + 1]);
            const auto c = point(indices[i + 2]);
            const auto low = (glm::min)(a, (glm::min)(b, c));
            const auto high = (glm::max)(a, (glm::max)(b, c));
            for (int x = static_cast<int>(std::floor(low.x / cell));
                x <= static_cast<int>(std::floor(high.x / cell)); ++x)
                for (int z = static_cast<int>(std::floor(low.z / cell));
                    z <= static_cast<int>(std::floor(high.z / cell)); ++z)
                    visit(std::make_pair(x, z), low.y, high.y);
        }
    };
    visitSurface(corpseVisual.animation, corpseModel, [&](const auto& key, float bottom, float top) {
        auto inserted = surface.emplace(key, Interval{bottom, top});
        inserted.first->second.first = (std::min)(inserted.first->second.first, bottom);
        inserted.first->second.second = (std::max)(inserted.first->second.second, top);
    });
    std::map<std::pair<int, int>, Interval> underside;
    lootPoseHeight = .75f;
    visitSurface(animation, lootModel, [&](const auto& key, float bottom, float top) {
        lootPoseHeight = (std::max)(lootPoseHeight, top - corpse.position.y + clearance);
        auto inserted = underside.emplace(key, Interval{bottom, top});
        inserted.first->second.first = (std::min)(inserted.first->second.first, bottom);
        inserted.first->second.second = (std::max)(inserted.first->second.second, top);
    });
    const char* supports[] = {"RightFoot", "LeftFoot", "RightLeg", "LeftLeg"};
    std::array<glm::vec3, 4> baseSupports{};
    for (size_t i = 0; i < baseSupports.size(); ++i) {
        baseSupports[i] = glm::vec3(lootModel * glm::vec4(animation.NodePosition(supports[i]), 1));
        baseSupports[i].y = corpse.position.y;
    }
    const float yaw = glm::radians(corpse.rotation.y);
    const glm::vec3 across(std::cos(yaw), 0, -std::sin(yaw));
    std::array<glm::vec3, 2> waistTargets{};
    for (size_t i = 0; i < waistTargets.size(); ++i) {
        // The player faces the enemy head from the hip/thigh side.
        // Each hand grips its corresponding side of the enemy waist.
        auto target = pelvis + across * (i == 0 ? .16f : -.16f);
        target.y = pelvis.y + .10f;
        const auto found = surface.find({static_cast<int>(std::floor(target.x / cell)),
            static_cast<int>(std::floor(target.z / cell))});
        if (found != surface.end()) target.y = found->second.second + .025f;
        waistTargets[i] = target;
    }
    // Search in the ground plane: moving upward would unplant the knees.
    // Integer cell offsets keep the conservative triangle coverage exact.
    int bestDistance = 1000000;
    glm::vec3 offset(0);
    for (int x = -50; x <= 50; ++x)
        for (int z = -50; z <= 50; ++z) {
            const int distance = x * x + z * z;
            if (distance >= bestDistance || distance > 100) continue;
            const glm::vec3 shift(x * cell, 0, z * cell);
            bool reachable = true;
            for (size_t i = 0; i < waistTargets.size(); ++i) {
                const std::string side = i == 0 ? "Right" : "Left";
                const auto joint = [&](const char* suffix) {
                    return glm::vec3(lootModel * glm::vec4(animation.NodePosition(side + suffix), 1)) + shift;
                };
                const float reach = glm::length(joint("ForeArm") - joint("Arm")) +
                    glm::length(joint("Hand") - joint("ForeArm"));
                if (glm::length(waistTargets[i] - joint("Arm")) > reach - .035f) reachable = false;
            }
            if (!reachable) continue;
            bool clear = true;
            for (const auto& entry : underside) {
                const auto found = surface.find({entry.first.first + x, entry.first.second + z});
                // Raised enemy limbs do not occupy the empty space below them.
                if (found != surface.end() &&
                    entry.second.first < found->second.second + clearance &&
                    entry.second.second > found->second.first - clearance) {
                    clear = false;
                    break;
                }
            }
            if (clear) {
                const glm::vec3 candidateOffset(x * cell, 0, z * cell);
                auto candidateSupports = baseSupports;
                for (auto& support : candidateSupports) support += candidateOffset;
                if (!placementClear(anchor + candidateOffset, lootPoseHeight, candidateSupports)) continue;
                bestDistance = distance;
                offset = candidateOffset;
                lootSupports = candidateSupports;
                lootPoseValid = true;
            }
        }
    if (!lootPoseValid) {
        // Prefer a checked placement, but always provide the waist-side pose
        // in cramped spaces rather than refusing the player's interaction.
        offset = glm::vec3(0);
        lootSupports = baseSupports;
        for (auto& support : lootSupports) support += offset;
        lootPoseValid = true;
    }
    {
        lootInverse = glm::inverse(glm::translate(glm::mat4(1), offset) * lootModel);
        lootClock = 0;
        for (size_t i = 0; i < waistTargets.size(); ++i) {
            lootHandTargets[i] = glm::vec3(lootInverse * glm::vec4(waistTargets[i], 1));
            lootElbows[i] = animation.NodePosition(i == 0 ? "RightForeArm" : "LeftForeArm");
        }
        lootPalmDirection = glm::vec3(lootInverse * glm::vec4(0, -1, 0, 0));
        lootSweepDirection = glm::vec3(lootInverse * glm::vec4(across, 0));
        // Always animate from the fitted pose so the sway cannot accumulate.
        animation.CapturePose();
        AnimateLootSearch(0);
    }
    return anchor + offset;
}

void CharacterVisual::AnimateLootSearch(float dt)
{
    if (!lootPoseValid) return;
    lootClock = std::fmod(lootClock + dt, 2.4f);
    // Move only the lower spine and torso; the fitted knees and feet stay still.
    animation.ApplyMotion(assets->lootSearchClip, lootClock, 1, false, true, false);
    animation.RefreshPose();
    for (size_t i = 0; i < lootHandTargets.size(); ++i) {
        const float phase = lootClock * 6.2831853f / 2.4f + i * 3.14159265f;
        const float active = (std::max)(0.f, std::sin(phase));
        const float lift = active * active;
        const auto motion = glm::vec3(lootInverse * glm::vec4(0, .012f * lift, 0, 0)) +
            lootSweepDirection * (.018f * lift * std::cos(phase));
        animation.SolveArm(i == 0 ? "Right" : "Left", lootHandTargets[i] + motion,
            lootElbows[i], lootPalmDirection, 1);
    }
    animation.RefreshPose();
}

void CharacterVisual::Reset()
{
    if (paletteTexture) glDeleteTextures(1, &paletteTexture);
    if (paletteBuffer) glDeleteBuffers(1, &paletteBuffer);
    paletteTexture = paletteBuffer = 0;
    assets.reset();
    poseElapsed = 0;
    backpackClock = backpackMotion = 0;
    lootPoseValid = false;
    lootPoseHeight = .75f;
    lootClock = 0;
    lootInverse = glm::mat4(1);
    lootHandTargets = lootElbows = {};
    lootPalmDirection = {0, -1, 0};
    lootSweepDirection = {};
    lootSupports = {};
    animation.Reset();
    weapon.Reset();
    weaponModel = glm::mat4(1);
    dying = false;
    firePending = false;
    deathTime = holdWeight = motionClock = aimWeight = 0;
    jumpWeight = jumpPhase = 0;
    landingTime = 1;
    wasAirborne = false;
    proneWeight = crawlClock = crawlWeight = 0;
    recoilTime = 1;
    stanceWeights = {};
    deathRoot = {};
    state = {};
}
