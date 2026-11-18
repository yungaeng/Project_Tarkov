#include "../Core/AssetCatalog.h"
#include "CharacterVisual.h"
#include "../Game/Character/Character.h"
#include "../Graphics/Renderer.h"
#include "../Graphics/Texture.h"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <array>
#include <vector>
#include <unordered_map>
#include <limits>

namespace
{
    struct CellKey
    {
        int x = 0;
        int z = 0;
        bool operator==(const CellKey& other) const noexcept { return x == other.x && z == other.z; }
    };

    struct CellKeyHash
    {
        std::size_t operator()(const CellKey& key) const noexcept
        {
            const std::size_t h1 = static_cast<std::size_t>(key.x >= 0 ? 2 * key.x : -2 * key.x - 1);
            const std::size_t h2 = static_cast<std::size_t>(key.z >= 0 ? 2 * key.z : -2 * key.z - 1);
            return h1 * 131071u + h2;
        }
    };

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
    MotionClip bedLootEnemyClip, bedLootPlayerClip, bedLootPlayerLoopClip, bedLootThirdClip, bedLootFourthClip;
    MotionClip deathClip, lootClip, lootSearchClip, hideoutKneelClip, reloadClip, aimClip, recoilClip;
    MotionClip crawlClip;
};

CharacterVisual::~CharacterVisual() { Reset(); }

void CharacterVisual::Init(PlayerAppearance appearance)
{
    Reset();
    const std::string folder = appearance == PlayerAppearance::Male ? AssetCatalog::Models_Player_Male : AssetCatalog::Models_Player_Female;
    if (!animation.LoadModel(AssetCatalog::Models_Player_Ch22_nonPBR_fbx) ||
        !animation.LoadAppearance(folder + "character.mesh") ||
        !animation.LoadClip("idle", AssetCatalog::Animations_Characters_Locomotion_Idle_fbx) ||
        !animation.LoadClip("walk", AssetCatalog::Animations_Characters_Locomotion_Walking_fbx) ||
        !animation.LoadClip("run", AssetCatalog::Animations_Characters_Locomotion_Fast_Run_fbx) ||
        !animation.LoadClip("crouch", AssetCatalog::Animations_Characters_Locomotion_Crouched_Walking_fbx))
        throw std::runtime_error(animation.Error());
    const auto load = [&](const std::string& id,const std::string& file) {
        if (!animation.LoadClip(id,std::string(AssetCatalog::ImportedMixamo)+file+".fbx"))
            throw std::runtime_error(animation.Error());
    };
    const char* directions[]={"forward","forward right","right","backward right","backward","backward left","left","forward left"};
    for(int i=0;i<8;++i) {
        for(const char* gait:{"walk","run","sprint"}) load(std::string(gait)+std::to_string(i),std::string(gait)+" "+directions[i]);
        load("crouch"+std::to_string(i),std::string("walk crouching ")+directions[i]);
    }
    for(const auto& entry:{std::pair<const char*,const char*>{"aim_idle","idle aiming"},
        {"crouch_idle","idle crouching"},{"crouch_aim","idle crouching aiming"},
        {"jump_start","jump up"},{"jump_loop","jump loop"},{"jump_land","jump down"},
        {"death_front","death from the front"},{"death_back","death from the back"},
        {"death_crouch","death crouching headshot front"},{"kneel","Rifle_Kneel_Idle"},
        {"kneel_aim","Rifle_Kneel_To_Aim"},{"prone","Prone_Idle"},{"prone_fire","Prone_Firing_Rifle"},
        {"prone_reload","Prone_Reloading"},{"equip","Rifle_Pull_Out"},{"holster","Rifle_Put_Away"},
        {"to_prone","Rifle_Kneel_To_Prone"},{"from_prone","Rifle_Prone_To_Kneel"}}) load(entry.first,entry.second);
    for(const auto& entry:{std::pair<const char*,const char*>{"unarmed_idle","Idle_Loop"},
        {"unarmed_walk","Walk_Loop"},{"unarmed_run","Sprint_Loop"},{"melee","Punch_Cross"},
        {"hit","Hit_Chest"}})
        if(!animation.LoadClip(entry.first,AssetCatalog::ImportedQuaternius,entry.second))
            throw std::runtime_error(animation.Error());
    static std::unordered_map<PlayerAppearance, std::weak_ptr<Assets>> caches;
    auto& cache = caches[appearance];
    assets = cache.lock();
    if (!assets)
    {
        auto shared = std::make_shared<Assets>();
        if (!shared->texture.Load((folder + "character.png").c_str()))
            throw std::runtime_error("Character texture load failed");
        CreateBackpackShell(shared->backpack);
        shared->backpackDetail.CreateCube();
        shared->deathClip.Load(AssetCatalog::Animations_Characters_Death_anim);
        shared->crawlClip.Load(AssetCatalog::Animations_Characters_ProneCrawl_anim);
        shared->lootClip.Load(AssetCatalog::Animations_Characters_CorpseLoot_anim);
        shared->lootSearchClip.Load(AssetCatalog::Animations_Characters_CorpseSearch_anim);
        shared->bedLootEnemyClip.Load(AssetCatalog::Animations_Characters_BedLootEnemy_anim);
        shared->bedLootPlayerClip.Load(AssetCatalog::Animations_Characters_BedLootPlayer_anim);
        shared->bedLootPlayerLoopClip.Load(AssetCatalog::Animations_Characters_BedLootPlayerLoop_anim);
        shared->bedLootThirdClip.Load(AssetCatalog::Animations_Characters_BedLootThird_anim);
        shared->bedLootFourthClip.Load(AssetCatalog::Animations_Characters_BedLootFourth_anim);
        shared->hideoutKneelClip.Load(AssetCatalog::Animations_Characters_HideoutKneel_anim);
        shared->reloadClip.Load(AssetCatalog::Animations_Characters_RifleReload_anim);
        shared->aimClip.Load(AssetCatalog::Animations_Characters_RifleAim_anim);
        shared->recoilClip.Load(AssetCatalog::Animations_Characters_RifleRecoil_anim);
        animation.CreateMesh(shared->mesh);
        assets = shared;
        cache = shared;
    }
    weapon.Init();
    animation.Update(0.15f, "idle", true, false);
    animation.RefreshPose();
    // Calibrate a neutral rifle socket even when the actor starts unarmed.
    // Deriving it from the first frame of a draw would bake a large hand offset.
    const glm::vec3 referenceScale(CharacterSettings{}.modelScale);
    const auto shoulder=animation.NodePosition("RightArm")*referenceScale+glm::vec3(.035f,-.035f,.08f);
    weaponModel=weapon.ModelMatrix(glm::translate(glm::mat4(1),shoulder),true);
    const auto rightGrip=glm::vec3(weaponModel*glm::vec4(WeaponVisual::FiringGrip(),1))/referenceScale;
    const auto leftGrip=glm::vec3(weaponModel*glm::vec4(weapon.SupportGrip(),1))/referenceScale;
    animation.SolveArm("Right",rightGrip,(shoulder+glm::vec3(-.18f,-.36f,.08f))/referenceScale,
        glm::vec3(weaponModel*glm::vec4(1,0,.4f,0))/referenceScale,1);
    animation.SolveArm("Left",leftGrip,(shoulder+glm::vec3(.30f,-.34f,.16f))/referenceScale,
        glm::vec3(weaponModel*glm::vec4(-.3f,0,1,0))/referenceScale,1);
    animation.RefreshPose();
    weaponHandAttachment=glm::inverse(glm::scale(glm::mat4(1),referenceScale)*animation.NodeTransform("RightHand"))*weaponModel;
    weaponAttachmentValid=true;
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
    weapon.SetMagazineInserted(state.magazineInserted);
    recoilTime = state.fired ? 0 : recoilTime + dt;
    firePending |= state.fired;
    hitPending |= state.hit;
    if (roomTwoLootRole != 0) {
        state.hasWeapon = state.equipped = false;
        weapon.Update(dt,false,false,0,false,false,true);
        animation.ResetToBindPose();
        const MotionClip& bedLootClip = roomTwoLootRole == 1 ? assets->bedLootEnemyClip :
            (roomTwoLootRole == 3 ? assets->bedLootThirdClip :
                (roomTwoLootRole == 4 ? assets->bedLootFourthClip : assets->bedLootPlayerClip));
        animation.ApplyMotion(bedLootClip,
            0,1,true,false);
        // Imported rigs do not share the Euler axes assumed by the original preview.
        // Establish the torso and leg directions in model space, parent before child.
        const bool enemy = roomTwoLootRole == 1;
        if (roomTwoLootRole != 3 && roomTwoLootRole != 4) {
            animation.AimBoneDirection("Hips","Spine",enemy ? glm::vec3(0,.12f,1) : glm::vec3(0,1,.08f));
            animation.AimBoneDirection("Spine","Spine1",enemy ? glm::vec3(0,0,1) : glm::vec3(0,1,.15f));
            animation.AimBoneDirection("Spine1","Spine2",enemy ? glm::vec3(0,0,1) : glm::vec3(0,1,.15f));
            animation.AimBoneDirection("Spine2","Neck",enemy ? glm::vec3(0,.08f,1) : glm::vec3(0,1,.1f));
            animation.AimBoneDirection("Neck","Head",enemy ? glm::vec3(0,.4f,1) : glm::vec3(0,1,.1f));
            for (const std::string side : {"Left", "Right"}) {
                const float kneeSpread = enemy ? (side == "Left" ? .20f : -.20f) : 0.f;
                animation.AimBoneDirection(side + "UpLeg",side + "Leg",
                    glm::normalize(glm::vec3(kneeSpread,-.98f,.08f)));
                animation.AimBoneDirection(side + "Leg",side + "Foot",glm::vec3(0,.12f,-1));
                animation.AimBoneDirection(side + "Foot",side + "ToeBase",glm::vec3(0,-.15f,-1));
            }
        }
        if (roomTwoLootRole == 2 && assets->bedLootPlayerLoopClip.Duration() > 0) {
            bedLootMotionTime = std::fmod(bedLootMotionTime + dt, assets->bedLootPlayerLoopClip.Duration());
            animation.ApplyMotion(assets->bedLootPlayerLoopClip,bedLootMotionTime,1.f,false,false,false);
        }
        if (enemy) {
            const float kneeY = .5f * (animation.NodePosition("LeftLeg").y + animation.NodePosition("RightLeg").y);
            for (const std::string side : {"Left", "Right"}) {
                const auto shoulder = animation.NodePosition(side + "Arm");
                const auto elbow = animation.NodePosition(side + "ForeArm");
                const float armLength = glm::length(elbow - shoulder) +
                    glm::length(animation.NodePosition(side + "Hand") - elbow);
                // Hands forward of the shoulders; elbows bend outward, fingers forward.
                const auto hand = glm::vec3(shoulder.x,kneeY,shoulder.z + armLength * .22f);
                const auto pole = shoulder + glm::vec3(side == "Left" ? armLength * .3f : -armLength * .3f,
                    -armLength * .5f,-armLength * .2f);
                animation.SolveArm(side,hand,pole,glm::vec3(0,0,1),1);
            }
        }
        animation.RefreshVertices();
        const auto scale = character.scale * character.GetSettings().modelScale;
        const auto hips = animation.NodePosition("Hips") * scale;
        float bottom = (std::numeric_limits<float>::max)();
        for (const auto& vertex : animation.Vertices())
            bottom = (std::min)(bottom,vertex.position.y * scale.y);
        if (!std::isfinite(bottom) || animation.Vertices().empty()) bottom = 0;
        roomTwoLootRoot = glm::translate(glm::mat4(1),glm::vec3(-hips.x,-bottom,-hips.z));
        UploadPalette();
        return;
    }
    if (hideoutBedPose) {
        weapon.Update(dt,false,false,0,false,false,true);
        if (!hideoutBedPoseInitialized) {
            animation.ResetToBindPose();
            animation.ApplyMotion(assets->deathClip,assets->deathClip.Duration(),1,true,false);
            hideoutBedRoot=assets->deathClip.Sample("Root",assets->deathClip.Duration()).Matrix();
            hideoutBedRoot[3].x=0.f;
            hideoutBedRoot[3].y=0.f;
            hideoutBedRoot[3].z=0.f;
            const auto head=animation.NodePosition("Head");
            const auto feet=(animation.NodePosition("RightFoot")+animation.NodePosition("LeftFoot"))*.5f;
            auto pointsHeadToPillow=[&](float yaw) {
                const auto candidate=glm::rotate(hideoutBedRoot,glm::radians(yaw),glm::vec3(0,1,0));
                const auto candidateHead=glm::vec3(candidate*glm::vec4(head,1));
                const auto candidateFeet=glm::vec3(candidate*glm::vec4(feet,1));
                return candidateHead.z<candidateFeet.z;
            };
            const float bedYaw=pointsHeadToPillow(0.f)?0.f:180.f;
            hideoutBedYaw=bedYaw;
            hideoutBedRoot=glm::rotate(hideoutBedRoot,glm::radians(bedYaw),glm::vec3(0,1,0));
            const auto localHead=glm::vec3(hideoutBedRoot*glm::vec4(head,1));
            const glm::vec3 modelScale=character.scale*character.GetSettings().modelScale;
            hideoutBedRoot[3].x=(hideoutBedCenter.x-character.position.x)/modelScale.x-localHead.x;
            hideoutBedRoot[3].z=(hideoutBedCenter.z-character.position.z-.78f)/modelScale.z-localHead.z;
            float minimum=(std::numeric_limits<float>::max)();
            for (const auto& vertex : animation.Vertices()) {
                const glm::vec3 position(vertex.position.x,vertex.position.y,vertex.position.z);
                minimum=(std::min)(minimum,
                    (hideoutBedRoot*glm::vec4(position*modelScale,1)).y);
            }
            hideoutBedRoot[3].y=(hideoutBedSurfaceHeight-character.position.y-minimum)/modelScale.y;
            hideoutBedPoseInitialized=true;
        }
        UploadPalette();
        return;
    }
    if (state.dead)
    {
        weapon.Update(dt, state.equipped, false, 0, false, false, true);
        if (!dying) {
            if(state.scriptedCorpsePose) animation.ResetToBindPose();
            animation.CapturePose(); dying=true; deathTime=0;lootCorpseFlipped=false;
            deathAnimation=(state.scriptedCorpsePose||character.IsProne())?"":character.IsCrouching()?"death_crouch":
                (static_cast<int>(std::abs(character.position.x+character.position.z))%2?"death_front":"death_back");
        }
        const float duration=deathAnimation.empty()?assets->deathClip.Duration():animation.ClipDuration(deathAnimation);
        if(deathTime>=duration) return;
        deathTime=(std::min)(duration,deathTime+dt);
        const auto previousHand = animation.NodePosition("RightHand");
        const float blend = std::clamp(deathTime / .65f, 0.f, 1.f);
        if(deathAnimation.empty())
            animation.ApplyMotion(assets->deathClip, deathTime, blend * blend * (3.f - 2.f * blend), true, true, false);
        else animation.ApplyClip(deathAnimation,deathTime,blend*blend*(3.f-2.f*blend),false,false,true);
        animation.RefreshPose();
        weaponModel[3] += glm::vec4((animation.NodePosition("RightHand") - previousHand) *
            character.scale * character.GetSettings().modelScale, 0);
        deathRoot = deathAnimation.empty()?assets->deathClip.Sample("Root",deathTime,true):MotionPose{};
        // Keep the rotated mesh above the feet/support plane, including crouched deaths.
        const auto rotation = MotionPose{glm::vec3(0), deathRoot.rotation}.Matrix();
        const float minimum = animation.MinimumHeight(glm::scale(rotation,
            character.scale * character.GetSettings().modelScale));
        deathRoot.position.y = (std::max)(deathRoot.position.y, -minimum + 0.01f);
        UploadPalette();
        return;
    }
    if (character.IsLooting()) {
        // Keep the knees planted while searching the waist pockets.
        if (hideoutKneelPose) {
            if (!hideoutKneelPoseInitialized) {
                animation.ResetToBindPose();
                animation.ApplyMotion(assets->hideoutKneelClip,0,1,true,false);
                animation.CapturePose();
                hideoutKneelPoseInitialized=true;
            }
            animation.RefreshPose();
        } else if (lootPreview) {
            // UI previews use the authored kneel/search clips without a world corpse or collision fit.
            if (!lootPreviewInitialized) {
                animation.ResetToBindPose();
                animation.ApplyMotion(assets->lootClip,0,1,true,false,true);
                animation.CapturePose();
                lootClock=0;
                lootPreviewInitialized=true;
            }
            lootClock=std::fmod(lootClock+dt,2.4f);
            animation.ApplyMotion(assets->lootSearchClip,lootClock,1,false,true,false);
            animation.RefreshPose();
        } else AnimateLootSearch(dt);
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
        landingTime = animation.ClipDuration("jump_land");
    }
    jumpWeight += ((airborne ? 1.f : 0.f) - jumpWeight) * (1.f - std::exp(-dt * 18.f));
    poseElapsed += dt;
    if (poseElapsed < poseInterval) return;
    dt = poseElapsed;
    poseElapsed = 0;
    const bool moving = character.IsMoving();
    const bool crouching = character.IsCrouching();
    const bool prone = character.IsProne();
    if(!equipmentInitialized) {equipmentInitialized=true;previousEquipped=state.equipped;previousProne=prone;}
    if(previousEquipped!=state.equipped) {handlingTime=0;previousEquipped=state.equipped;}
    else handlingTime+=dt;
    if(previousProne!=prone) {postureTime=0;postureClip=prone?"to_prone":"from_prone";previousProne=prone;}
    else postureTime+=dt;
    hitTime=hitPending?0:hitTime+dt;hitPending=false;
    const bool handling=handlingTime<WeaponHandlingSeconds && state.hasWeapon;
    weapon.Update(dt,state.equipped,state.aiming,state.reloadRemaining,firePending,moving,false,
        character.IsSprinting()&&moving&&!state.aiming,crouching,true,WeaponHandlingSeconds);
    firePending=false;
    const auto& movement = character.GetSettings();
    const float referenceSpeed = prone ? movement.proneSpeed : crouching ? movement.crouchSpeed :
        character.IsSprinting() ? movement.runSpeed : movement.walkSpeed;
    const float pace = moving ? std::clamp(character.GetMoveSpeed() / referenceSpeed, 0.f, 1.5f) : 1.f;
    const auto direction=glm::vec3(glm::rotate(glm::mat4(1),glm::radians(-character.rotation.y),glm::vec3(0,1,0))*
        glm::vec4(character.GetMoveDirection(),0));
    const int sector=(static_cast<int>(std::floor(std::atan2(direction.x,direction.z)/.785398163f+.5f))+8)%8;
    const bool sprint=moving&&character.IsSprinting()&&!crouching&&!prone&&!state.aiming;
    std::string clip;
    if(prone) clip="prone";
    else if(crouching) clip=moving?"crouch"+std::to_string(sector):(state.equipped?"kneel":"crouch_idle");
    else if(!state.equipped && !handling) clip=moving?(character.IsSprinting()?"unarmed_run":"unarmed_walk"):"unarmed_idle";
    else if(moving) clip=std::string(sprint?"sprint":character.GetMoveSpeed()>movement.walkSpeed*1.1f?"run":"walk")+std::to_string(sector);
    else clip=state.aiming?"aim_idle":"idle";
    animation.Update(dt,clip,true,false,moving?pace:1.f);
    const float blend = 1 - std::exp(-dt * 12);
    proneWeight += ((prone ? 1.f : 0.f) - proneWeight) * (1.f - std::exp(-dt * 7.f));
    crawlWeight += ((prone && moving ? 1.f : 0.f) - crawlWeight) * blend;
    if (prone && moving) crawlClock = std::fmod(crawlClock + dt * pace, assets->crawlClip.Duration());
    holdWeight += (((state.equipped||handling) ? 1.0f : 0.0f) - holdWeight) * blend;
    motionClock = std::fmod(motionClock + dt, 924.0f);
    aimWeight += ((state.aiming && !sprint && state.reloadRemaining <= 0 ? 1.f : 0.f) - aimWeight) * blend;
    const float postureDuration=animation.ClipDuration(postureClip);
    if(postureTime<postureDuration && !moving && !airborne) {
        const float weight=std::clamp((std::min)(postureTime,postureDuration-postureTime)/.12f,0.f,1.f);
        animation.ApplyClip(postureClip,postureTime,weight);
    }
    if(crouching&&!moving&&state.aiming)
        animation.ApplyClip("kneel_aim",animation.ClipDuration("kneel_aim"),aimWeight,true);
    if(crouching&&moving&&state.aiming)
        animation.ApplyClip("crouch_aim",motionClock,aimWeight,true,true);
    animation.ApplyMotion(assets->crawlClip, crawlClock, crawlWeight * proneWeight, false, false, false);
    animation.ApplyMotion(assets->aimClip, 0, aimWeight * holdWeight, false, false, false);
    const bool proneShot=prone&&recoilTime<animation.ClipDuration("prone_fire");
    if(proneShot) animation.ApplyClip("prone_fire",recoilTime,holdWeight,true);
    else animation.ApplyMotion(assets->recoilClip,recoilTime,holdWeight,false,false,false);
    if(state.reloadRemaining>0) {
        if(prone) animation.ApplyClip("prone_reload",(1.f-state.reloadRemaining/2.f)*animation.ClipDuration("prone_reload"),holdWeight,true);
        else animation.ApplyMotion(assets->reloadClip,2-state.reloadRemaining,holdWeight,false,false,false);
    }
    if(airborne) {
        const std::string jump=jumpPhase<.5f?"jump_start":"jump_loop";
        animation.ApplyClip(jump,jumpPhase*animation.ClipDuration(jump),jumpWeight*(1.f-proneWeight));
    }
    if(!airborne&&!prone&&landingTime<animation.ClipDuration("jump_land"))
        animation.ApplyClip("jump_land",landingTime,1.f);
    if(handling) {
        const std::string action=state.equipped?"equip":"holster";
        const float phase=handlingTime/WeaponHandlingSeconds;
        const float weight=std::clamp((state.equipped?(std::min)(phase,1.f-phase):phase)*10.f,0.f,1.f);
        animation.ApplyClip(action,phase*animation.ClipDuration(action),weight,true);
    }
    if(state.meleeRemaining>0)
        animation.ApplyClip("melee",(1.f-state.meleeRemaining/MeleeSeconds)*animation.ClipDuration("melee"),1.f,true);
    const float hitDuration=animation.ClipDuration("hit");
    if(hitTime<hitDuration) animation.ApplyClip("hit",hitTime,.65f*(1.f-hitTime/hitDuration),true);
    animation.RefreshPose();
    const bool authoredArms=handling||proneShot||(prone&&state.reloadRemaining>0)||state.meleeRemaining>0;
    const float armIK=authoredArms?0.f:hitTime<hitDuration?.4f:holdWeight;
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
        const float compact = (1.f - proneWeight) * (state.reloadRemaining > 0 ? 0.f : 1.f);
        auto rightElbow = glm::mix(glm::vec3(-.3f, -.4f, .1f), glm::vec3(-.18f, -.36f, .08f), compact);
        auto leftElbow = glm::mix(glm::vec3(.5f, -.4f, .1f), glm::vec3(.30f, -.34f, .16f), compact);
        rightElbow = glm::mix(shoulder + rightElbow, glm::vec3(-.24f, .12f, .32f), proneWeight);
        leftElbow = glm::mix(shoulder + leftElbow, glm::vec3(.30f, .12f, .32f), proneWeight);
        animation.SolveArm("Right",right,rightElbow/scale,rightPalm,armIK);
        animation.SolveArm("Left",left,leftElbow/scale,leftPalm,armIK);
        animation.RefreshPose();
        const auto hand=glm::scale(glm::mat4(1),scale)*animation.NodeTransform("RightHand");
        if(authoredArms && weaponAttachmentValid) weaponModel=hand*weaponHandAttachment;
        else {weaponHandAttachment=glm::inverse(hand)*weaponModel;weaponAttachmentValid=true;}
    }
    UploadPalette();
}

bool CharacterVisual::MeleeStrikeSegment(const Character& character,glm::vec3& torso,glm::vec3& fist,float& radius) const
{
    if(!assets||state.dead||state.meleeRemaining<=0) return false;
    const auto actor=glm::translate(glm::mat4(1),character.position)*
        glm::rotate(glm::mat4(1),glm::radians(character.rotation.y),glm::vec3(0,1,0))*
        glm::scale(glm::mat4(1),character.scale*character.GetSettings().modelScale);
    auto world=[&](const char* node){return glm::vec3(actor*glm::vec4(animation.NodePosition(node),1));};
    torso=world("Spine2");
    const auto wrist=world("RightHand"),knuckle=world("RightHandMiddle1");
    fist=knuckle;
    radius=std::clamp(glm::length(knuckle-wrist)*.65f,.025f,.085f);
    const float reach=glm::length(fist-torso);
    return std::isfinite(reach)&&reach>.01f&&std::isfinite(radius);
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
    if (roomTwoLootRole != 0) model *= roomTwoLootRoot;
    else if (dying) model *= CorpseRootMatrix();
    else if (hideoutBedPose) model *= hideoutBedRoot;
    else if (character.IsLooting()) {
        auto root = hideoutKneelPose
            ? assets->hideoutKneelClip.Sample("Root",0)
            : assets->lootClip.Sample("Root", 0);
        model *= root.Matrix();
    }
    const glm::mat4 actor = model;
    model = glm::scale(model, character.scale * character.GetSettings().modelScale);
    // Bone envelopes conservatively enclose the current skinned pose.
    // Padding also covers the attached rifle, magazine and muzzle flash.
    const float weaponPadding = 2.0f;
    if (!hideoutBedPose && !character.IsLooting() &&
        !Renderer::IsVisible(animation.BoundsMin(), animation.BoundsMax(), model, weaponPadding)) return;
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
    const std::function<bool(const glm::vec3&, float, const std::array<glm::vec3, 4>&)>& placementClear,
    float approachYaw,float supportHeight)
{
    animation.ResetToBindPose();
    animation.ApplyMotion(assets->lootClip, 0, 1, true, false, true);
    lootPoseValid = false;
    lootPoseYaw = corpse.rotation.y +
        (corpseVisual.hideoutBedPose ? corpseVisual.hideoutBedYaw : 0.f) + approachYaw;
    auto corpseModel = glm::translate(glm::mat4(1), corpse.position);
    corpseModel = glm::rotate(corpseModel, glm::radians(corpse.rotation.y), glm::vec3(0, 1, 0));
    corpseModel *= corpseVisual.CorpseRootMatrix();
    corpseModel = glm::scale(corpseModel, corpse.scale * corpse.GetSettings().modelScale);
    const float corpseYaw=corpse.rotation.y+
        (corpseVisual.hideoutBedPose?corpseVisual.hideoutBedYaw:0.f);
    const float groundHeight=supportHeight>=0.f?supportHeight:corpse.position.y;
    auto lootModel = glm::rotate(glm::mat4(1), glm::radians(lootPoseYaw), glm::vec3(0, 1, 0));
    lootModel *= assets->lootClip.Sample("Root", 0).Matrix();
    lootModel = glm::scale(lootModel, character.scale * character.GetSettings().modelScale);
    const auto pelvis = glm::vec3(corpseModel * glm::vec4(corpseVisual.animation.NodePosition("Hips"), 1));
    // The living player kneels toward the supine enemy head.
    // Align the reach of the authored kneel, not the two pelvis joints.
    // Pelvis alignment put the looter directly on top of the corpse.
    const auto handCenter = (animation.NodePosition("RightHand") + animation.NodePosition("LeftHand")) * .5f;
    const auto localHands = glm::vec3(lootModel * glm::vec4(handCenter, 1));
    const glm::vec3 anchor(pelvis.x - localHands.x, groundHeight, pelvis.z - localHands.z);
    lootModel = glm::translate(glm::mat4(1), anchor) * lootModel;
    // Preserve the authored folded shins instead of rotating each thigh alone.

    // Build a padded surface envelope once on entry. Include clothing so that
    // fitting the pelvis also preserves room for the torso and raised thighs.
    constexpr float cell = .04f;
    constexpr float clearance = .02f;
    using Interval = std::pair<float, float>;
    std::unordered_map<CellKey, Interval, CellKeyHash> surface;
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
                    visit(CellKey{x, z}, low.y, high.y);
        }
    };
    visitSurface(corpseVisual.animation, corpseModel, [&](const auto& key, float bottom, float top) {
        auto inserted = surface.emplace(key, Interval{bottom, top});
        inserted.first->second.first = (std::min)(inserted.first->second.first, bottom);
        inserted.first->second.second = (std::max)(inserted.first->second.second, top);
    });
    std::unordered_map<CellKey, Interval, CellKeyHash> underside;
    lootPoseHeight = .75f;
    visitSurface(animation, lootModel, [&](const auto& key, float bottom, float top) {
        lootPoseHeight = (std::max)(lootPoseHeight, top - groundHeight + clearance);
        auto inserted = underside.emplace(key, Interval{bottom, top});
        inserted.first->second.first = (std::min)(inserted.first->second.first, bottom);
        inserted.first->second.second = (std::max)(inserted.first->second.second, top);
    });
    const char* supports[] = {"RightFoot", "LeftFoot", "RightLeg", "LeftLeg"};
    std::array<glm::vec3, 4> baseSupports{};
    for (size_t i = 0; i < baseSupports.size(); ++i) {
        baseSupports[i] = glm::vec3(lootModel * glm::vec4(animation.NodePosition(supports[i]), 1));
        baseSupports[i].y = groundHeight;
    }
    const float yaw = glm::radians(corpseYaw);
    const glm::vec3 across(std::cos(yaw), 0, -std::sin(yaw));
    std::array<glm::vec3, 2> waistTargets{};
    for (size_t i = 0; i < waistTargets.size(); ++i) {
        // The player faces the enemy head from the hip/thigh side.
        // Each hand grips its corresponding side of the enemy waist.
        auto target = pelvis + across * (i == 0 ? .16f : -.16f);
        target.y = pelvis.y + .10f;
        const auto found = surface.find(CellKey{
            static_cast<int>(std::floor(target.x / cell)),
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
                const auto found = surface.find(CellKey{entry.first.x + x, entry.first.z + z});
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

void CharacterVisual::CenterDeathPoseOnSurface(Character& character,const glm::vec3& surfaceCenter) const
{
    if (!assets || animation.Vertices().empty()) return;
    auto model=glm::rotate(glm::mat4(1),glm::radians(character.rotation.y),glm::vec3(0,1,0));
    model*=CorpseRootMatrix();
    model=glm::scale(model,character.scale*character.GetSettings().modelScale);
    glm::vec3 low((std::numeric_limits<float>::max)());
    glm::vec3 high((std::numeric_limits<float>::lowest)());
    for (const auto& vertex:animation.Vertices()) {
        const auto point=glm::vec3(model*glm::vec4(vertex.position.x,vertex.position.y,vertex.position.z,1));
        low=(glm::min)(low,point);
        high=(glm::max)(high,point);
    }
    // Center the visible body, rather than its standing animation origin.
    character.position=surfaceCenter-glm::vec3((low.x+high.x)*.5f,low.y,(low.z+high.z)*.5f);
}

void CharacterVisual::AlignDeathPosePelvis(Character& character,const glm::vec3& worldPosition) const
{
    const auto modelScale=character.scale*character.GetSettings().modelScale;
    const auto local=glm::vec3(CorpseRootMatrix()*
        glm::vec4(animation.NodePosition("Hips")*modelScale,1));
    const auto rotation=glm::rotate(glm::mat4(1),glm::radians(character.rotation.y),
        glm::vec3(0,1,0));
    character.position=worldPosition-glm::vec3(rotation*glm::vec4(local,0));
}

void CharacterVisual::AlignLootPosePelvis(Character& character,const glm::vec3& worldPosition) const
{
    const auto modelScale=character.scale*character.GetSettings().modelScale;
    const auto root=assets->lootClip.Sample("Root",0).Matrix();
    const auto local=glm::vec3(root*
        glm::vec4(animation.NodePosition("Hips")*modelScale,1));
    const auto rotation=glm::rotate(glm::mat4(1),glm::radians(character.rotation.y),
        glm::vec3(0,1,0));
    character.position=worldPosition-glm::vec3(rotation*glm::vec4(local,0));
}

void CharacterVisual::AlignLootPoseHead(Character& character,const glm::vec3& worldPosition) const
{
    const auto modelScale=character.scale*character.GetSettings().modelScale;
    const auto root=assets->lootClip.Sample("Root",0).Matrix();
    const auto local=glm::vec3(root*
        glm::vec4(animation.NodePosition("Head")*modelScale,1));
    const auto rotation=glm::rotate(glm::mat4(1),glm::radians(character.rotation.y),
        glm::vec3(0,1,0));
    character.position=worldPosition-glm::vec3(rotation*glm::vec4(local,0));
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

void CharacterVisual::SetLootPreview(bool enabled)
{
    if (lootPreview==enabled) return;
    lootPreview=enabled;
    lootPreviewInitialized=false;
}

void CharacterVisual::SetHideoutBedPose(bool enabled,float surfaceHeight,const glm::vec3& bedCenter)
{
    if (hideoutBedPose==enabled && hideoutBedSurfaceHeight==surfaceHeight &&
        hideoutBedCenter==bedCenter) return;
    hideoutBedPose=enabled;
    hideoutBedSurfaceHeight=surfaceHeight;
    hideoutBedCenter=bedCenter;
    if (enabled) {
        dying=false;
        deathTime=0.f;
        lootCorpseFlipped=false;
    } else hideoutBedYaw=0.f;
    hideoutBedPoseInitialized=false;
}

void CharacterVisual::SetRoomTwoLootPose(int role)
{
    const int nextRole = std::clamp(role,0,4);
    if (nextRole != roomTwoLootRole) bedLootMotionTime = 0.f;
    roomTwoLootRole = nextRole;
    roomTwoLootRoot = glm::mat4(1);
}

void CharacterVisual::FitRoomTwoLootHands(const Character& character, const Character& target,
    const CharacterVisual& targetVisual)
{
    if (!assets || !targetVisual.assets || roomTwoLootRole != 2 || targetVisual.roomTwoLootRole != 1) return;
    const auto actor = glm::translate(glm::mat4(1),character.position) *
        glm::rotate(glm::mat4(1),glm::radians(character.rotation.y),glm::vec3(0,1,0)) * roomTwoLootRoot *
        glm::scale(glm::mat4(1),character.scale * character.GetSettings().modelScale);
    const auto other = glm::translate(glm::mat4(1),target.position) *
        glm::rotate(glm::mat4(1),glm::radians(target.rotation.y),glm::vec3(0,1,0)) * targetVisual.roomTwoLootRoot *
        glm::scale(glm::mat4(1),target.scale * target.GetSettings().modelScale);
    const auto hips = glm::vec3(other * glm::vec4(targetVisual.animation.NodePosition("Hips"),1));
    for (const std::string side : {"Left", "Right"}) {
        // Search the equipment at either side of the waist; keep the pelvis still.
        const auto hand = glm::vec3(glm::inverse(actor) * glm::vec4(hips + glm::vec3(side == "Left" ? .16f : -.16f,.12f,0),1));
        const auto pole = animation.NodePosition(side + "ForeArm");
        animation.SolveArm(side,hand,pole,glm::vec3(0,-1,0),1);
    }
    animation.RefreshVertices();
    UploadPalette();
}

void CharacterVisual::SetHideoutKneelPose(bool enabled)
{
    if (hideoutKneelPose==enabled) return;
    hideoutKneelPose=enabled;
    hideoutKneelPoseInitialized=false;
    lootPoseValid=false;
}

void CharacterVisual::SetLootCorpseFlipped(bool enabled,const Character& character)
{
    lootCorpseFlipped=enabled;
    if (!enabled || !assets) return;
    flippedDeathRoot=deathRoot.Matrix();
    flippedDeathRoot[3].y=0.f;
    // Death.anim already lays the body on its back. Roll around the body's
    // longitudinal axis so the corpse turns over instead of spinning end-for-end.
    flippedDeathRoot=glm::rotate(flippedDeathRoot,glm::radians(180.f),glm::vec3(0,1,0));
    float minimum=0.f;
    for (const auto& vertex : animation.Vertices()) {
        const glm::vec3 position(vertex.position.x,vertex.position.y,vertex.position.z);
        minimum=(std::min)(minimum,
            (flippedDeathRoot*glm::vec4(position*character.scale*character.GetSettings().modelScale,1)).y);
    }
    flippedDeathRoot[3].y=-minimum+.01f;
}

glm::mat4 CharacterVisual::CorpseRootMatrix() const
{
    if (hideoutBedPose) return hideoutBedRoot;
    return lootCorpseFlipped ? flippedDeathRoot : deathRoot.Matrix();
}

void CharacterVisual::Reset()
{
    lootPreview=lootPreviewInitialized=false;
    if (paletteTexture) glDeleteTextures(1, &paletteTexture);
    if (paletteBuffer) glDeleteBuffers(1, &paletteBuffer);
    paletteTexture = paletteBuffer = 0;
    assets.reset();
    lootCorpseFlipped=false;
    flippedDeathRoot=glm::mat4(1);
    roomTwoLootRole=0;
    bedLootMotionTime=0.f;
    roomTwoLootRoot=glm::mat4(1);
    hideoutBedPose=hideoutBedPoseInitialized=false;
    hideoutBedSurfaceHeight=0.f;
    hideoutBedYaw=0.f;
    hideoutBedCenter=glm::vec3(0);
    hideoutBedRoot=glm::mat4(1);
    hideoutKneelPose=hideoutKneelPoseInitialized=false;
    poseElapsed = 0;
    backpackClock = backpackMotion = 0;
    lootPoseValid = false;
    lootPoseHeight = .75f;
    lootPoseYaw = 180.f;
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
    hitPending=false;
    equipmentInitialized=previousEquipped=previousProne=weaponAttachmentValid=false;
    weaponHandAttachment=glm::mat4(1);
    handlingTime=postureTime=hitTime=100;
    postureClip.clear();deathAnimation.clear();
    deathTime = holdWeight = motionClock = aimWeight = 0;
    jumpWeight = jumpPhase = 0;
    landingTime = 1;
    wasAirborne = false;
    proneWeight = crawlClock = crawlWeight = 0;
    recoilTime = 1;
    deathRoot = {};
    state = {};
}
