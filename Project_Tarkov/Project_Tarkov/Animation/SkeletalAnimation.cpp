#include "SkeletalAnimation.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/config.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <limits>
#include <fstream>
#include <cstdint>

namespace
{
    std::string NodeName(const aiString& name)
    {
        std::string result = name.C_Str();
        const auto colon = result.find_last_of(':');
        return colon == std::string::npos ? result : result.substr(colon + 1);
    }
    struct Pose
    {
        aiVector3D scale{ 1, 1, 1 }, position;
        aiQuaternion rotation;
    };
    Pose Blend(const Pose& a, const Pose& b, float t)
    {
        Pose result;
        result.scale = a.scale + (b.scale - a.scale) * t;
        result.position = a.position + (b.position - a.position) * t;
        aiQuaternion::Interpolate(result.rotation, a.rotation, b.rotation, t);
        result.rotation.Normalize();
        return result;
    }
    template<class Key, class Value, class Interpolate>
    Value Sample(const std::vector<Key>& keys, double time, const Value& fallback, Interpolate interpolate)
    {
        if (keys.empty()) return fallback;
        if (keys.size() == 1 || time <= keys.front().mTime) return keys.front().mValue;
        if (time >= keys.back().mTime) return keys.back().mValue;
        auto next = std::upper_bound(keys.begin(), keys.end(), time,
            [](double t, const Key& key) { return t < key.mTime; });
        const auto& previous = *(next - 1);
        const double span = next->mTime - previous.mTime;
        const float alpha = span > 0 ? static_cast<float>((time - previous.mTime) / span) : 0;
        return interpolate(previous.mValue, next->mValue, alpha);
    }
    aiVector3D Lerp(const aiVector3D& a, const aiVector3D& b, float t) { return a + (b - a) * t; }
    aiQuaternion Slerp(const aiQuaternion& a, const aiQuaternion& b, float t)
    {
        aiQuaternion result;
        aiQuaternion::Interpolate(result, a, b, t);
        return result.Normalize();
    }
    void Configure(Assimp::Importer& importer)
    {
        // These FBXs contain large embedded textures; animation does not need them.
        importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_TEXTURES | aiComponent_MATERIALS);
        importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    }
}

struct SkeletalAnimation::Data
{
    struct Node { std::string name; int parent; Pose bind; };
    struct Bone { size_t node; aiMatrix4x4 offset; };
    struct Weight { size_t bone; float value; };
    struct Source { AnimatedVertex vertex; size_t node; std::vector<Weight> weights; int garment = 0; };
    struct Track
    {
        std::vector<aiVectorKey> positions, scales;
        std::vector<aiQuatKey> rotations;
    };
    struct Clip { double duration = 0, rate = 25; std::unordered_map<size_t, Track> tracks; };
    struct Asset
    {
        std::vector<Node> nodes;
        std::unordered_map<std::string, size_t> nodeIds;
        std::vector<Bone> bones;
        std::vector<Source> sources;
        std::vector<unsigned int> indices;
        std::unordered_map<std::string, Clip> clips;
        std::unordered_map<std::string, std::string> clipPaths;
        std::vector<aiVector3D> animationBindPositions;
        aiMatrix4x4 inverseRoot;
        std::vector<glm::vec3> boundsMin, boundsMax;
    };
    std::shared_ptr<Asset> asset = std::make_shared<Asset>();
    std::vector<AnimatedVertex> output;
    std::vector<Pose> pose, transition, captured;
    std::vector<aiMatrix4x4> globals, palette;
    std::vector<aiMatrix3x3> normals;
    std::vector<glm::mat4> gpuPalette;
    glm::vec3 boundsMin{0}, boundsMax{0};
    std::string error, selected;
    double time = 0;
    double idleTime = 0;
    float blendTime = 0.15f;
    bool playing = false;

    void RefreshGlobals()
    {
        const auto& nodes = asset->nodes;
        globals.resize(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto& p = pose[i];
            const aiMatrix4x4 local(p.scale, p.rotation, p.position);
            globals[i] = nodes[i].parent < 0 ? local : globals[nodes[i].parent] * local;
        }
    }

    void RefreshPose()
    {
        RefreshGlobals();
        const auto& nodes = asset->nodes;
        const size_t count = asset->bones.size() + nodes.size();
        palette.resize(count);
        normals.resize(count);
        gpuPalette.resize(count * 2);
        boundsMin = glm::vec3((std::numeric_limits<float>::max)());
        boundsMax = -boundsMin;
        for (size_t i = 0; i < count; ++i)
        {
            if (i < asset->bones.size())
            {
                const auto& bone = asset->bones[i];
                palette[i] = asset->inverseRoot * globals[bone.node] * bone.offset;
            }
            else palette[i] = asset->inverseRoot * globals[i - asset->bones.size()];
            normals[i] = aiMatrix3x3(palette[i]);
            normals[i].Inverse().Transpose();
            // Assimp is row-major; GLM indexes columns first.
            glm::mat4 matrix(1), normal(1);
            for (int row = 0; row < 4; ++row)
                for (int col = 0; col < 4; ++col) matrix[col][row] = palette[i][row][col];
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col) normal[col][row] = normals[i][row][col];
            gpuPalette[i * 2] = matrix;
            gpuPalette[i * 2 + 1] = normal;
            if (asset->boundsMin.empty() || asset->boundsMin[i].x > asset->boundsMax[i].x) continue;
            for (int corner = 0; corner < 8; ++corner)
            {
                glm::vec3 point;
                for (int axis = 0; axis < 3; ++axis)
                    point[axis] = (corner & (1 << axis)) ? asset->boundsMax[i][axis] : asset->boundsMin[i][axis];
                point = glm::vec3(matrix * glm::vec4(point, 1));
                boundsMin = (glm::min)(boundsMin, point);
                boundsMax = (glm::max)(boundsMax, point);
            }
        }
    }

    // CPU skinning is retained only for precise corpse support.
    void Skin()
    {
        RefreshPose();
        output.resize(asset->sources.size());
        for (size_t i = 0; i < asset->sources.size(); ++i)
        {
            const auto& source = asset->sources[i];
            auto& vertex = output[i];
            vertex = source.vertex;
            if (source.weights.empty())
            {
                const size_t bone = asset->bones.size() + source.node;
                vertex.position = palette[bone] * source.vertex.position;
                vertex.normal = normals[bone] * source.vertex.normal;
            }
            else
            {
                vertex.position = aiVector3D();
                vertex.normal = aiVector3D();
                float sum = 0;
                for (const auto& weight : source.weights)
                {
                    vertex.position += (palette[weight.bone] * source.vertex.position) * weight.value;
                    vertex.normal += (normals[weight.bone] * source.vertex.normal) * weight.value;
                    sum += weight.value;
                }
                vertex.position /= sum;
                vertex.normal /= sum;
            }
            if (vertex.normal.SquareLength() > 0) vertex.normal.Normalize();
        }
    }

};

SkeletalAnimation::SkeletalAnimation() : data(std::make_unique<Data>()) {}
SkeletalAnimation::~SkeletalAnimation() = default;
void SkeletalAnimation::Reset() { data = std::make_unique<Data>(); }
const std::string& SkeletalAnimation::Error() const { return data->error; }
size_t SkeletalAnimation::BoneCount() const { return data->asset->bones.size(); }
const std::vector<AnimatedVertex>& SkeletalAnimation::Vertices() const { return data->output; }

void SkeletalAnimation::CapturePose() { data->captured = data->pose; }

void SkeletalAnimation::RefreshVertices() { data->Skin(); }

void SkeletalAnimation::ApplyMotion(const MotionClip& clip, float time, float weight, bool fromBind, bool frozen, bool skin)
{
    if (frozen && data->captured.size() == data->pose.size()) data->pose = data->captured;
    for (const auto& entry : clip.Tracks())
    {
        const auto node = data->asset->nodeIds.find(entry.first);
        if (node == data->asset->nodeIds.end()) continue; // Root is a world-space visual track.
        const size_t id = node->second;
        const auto key = clip.Sample(entry.first, time);
        Pose target = fromBind ? data->asset->nodes[id].bind : data->pose[id];
        target.position += aiVector3D(key.position.x, key.position.y, key.position.z);
        const auto r = glm::radians(key.rotation);
        target.rotation = target.rotation * aiQuaternion(aiVector3D(0, 0, 1), r.z) *
            aiQuaternion(aiVector3D(0, 1, 0), r.y) * aiQuaternion(aiVector3D(1, 0, 0), r.x);
        target.rotation.Normalize();
        data->pose[id] = Blend(data->pose[id], target, std::clamp(weight, 0.0f, 1.0f));
    }
    if (skin) data->Skin();
}

glm::vec3 SkeletalAnimation::NodePosition(const std::string& name) const
{
    const auto node = data->asset->nodeIds.find(name);
    if (node == data->asset->nodeIds.end() || data->globals.empty()) return glm::vec3(0);
    const auto point = (data->asset->inverseRoot * data->globals[node->second]) * aiVector3D();
    return {point.x, point.y, point.z};
}

void SkeletalAnimation::SolveArm(const std::string& side, const glm::vec3& target, const glm::vec3& pole,
    const glm::vec3& palmDirection, float weight)
{
    const auto upper = data->asset->nodeIds.find(side + "Arm");
    const auto lower = data->asset->nodeIds.find(side + "ForeArm");
    const auto hand = data->asset->nodeIds.find(side + "Hand");
    if (upper == data->asset->nodeIds.end() || lower == data->asset->nodeIds.end() ||
        hand == data->asset->nodeIds.end() || weight <= 0) return;
    // Solve in imported scene space, preserving the original bone lengths.
    auto root = data->asset->inverseRoot;
    root.Inverse();
    auto convert = [&](const glm::vec3& p) {
        const auto v = root * aiVector3D(p.x, p.y, p.z);
        return glm::vec3(v.x, v.y, v.z);
    };
    auto position = [&](size_t id) {
        const auto v = data->globals[id] * aiVector3D();
        return glm::vec3(v.x, v.y, v.z);
    };
    const glm::vec3 shoulder = position(upper->second);
    const glm::vec3 elbow = position(lower->second);
    const glm::vec3 wrist = position(hand->second);
    const float a = glm::length(elbow - shoulder), b = glm::length(wrist - elbow);
    if (a < 0.001f || b < 0.001f) return;
    const glm::vec3 desired = glm::mix(wrist, convert(target), std::clamp(weight, 0.f, 1.f));
    const float distance = glm::length(desired - shoulder);
    if (distance < 0.001f) return;
    const glm::vec3 forward = (desired - shoulder) / distance;
    const float reach = std::clamp(distance, std::abs(a - b) + 0.001f, a + b - 0.001f);
    glm::vec3 bend = convert(pole) - shoulder;
    bend -= forward * glm::dot(bend, forward);
    if (glm::length(bend) < 0.001f) {
        bend = glm::cross(forward, glm::vec3(0, 1, 0));
        if (glm::length(bend) < 0.001f) bend = glm::cross(forward, glm::vec3(1, 0, 0));
    }
    const float along = (a * a - b * b + reach * reach) / (2 * reach);
    const glm::vec3 elbowTarget = shoulder + forward * along + glm::normalize(bend) *
        std::sqrt((std::max)(0.f, a * a - along * along));
    auto align = [&](size_t joint, size_t child, const glm::vec3& destination) {
        const auto origin = position(joint);
        const auto from = glm::normalize(position(child) - origin);
        const auto to = glm::normalize(destination - origin);
        const float cosine = std::clamp(glm::dot(from, to), -1.f, 1.f);
        auto axis = glm::cross(from, to);
        if (glm::length(axis) < 0.00001f) {
            if (cosine > 0) return;
            axis = glm::cross(from, glm::vec3(0, 1, 0));
            if (glm::length(axis) < 0.00001f) axis = glm::cross(from, glm::vec3(1, 0, 0));
        }
        axis = glm::normalize(axis);
        const aiQuaternion delta(aiVector3D(axis.x, axis.y, axis.z), std::acos(cosine));
        aiQuaternion parentRotation;
        aiVector3D scale, translation;
        const int parent = data->asset->nodes[joint].parent;
        if (parent >= 0) data->globals[parent].Decompose(scale, parentRotation, translation);
        auto inverseParent = parentRotation;
        inverseParent.Conjugate();
        data->pose[joint].rotation = inverseParent * delta * parentRotation * data->pose[joint].rotation;
        data->pose[joint].rotation.Normalize();
        data->RefreshGlobals();
    };
    align(upper->second, lower->second, elbowTarget);
    align(lower->second, hand->second, shoulder + forward * reach);
    const auto finger = data->asset->nodeIds.find(side + "HandMiddle1");
    if (finger != data->asset->nodeIds.end()) {
        const auto direction = convert(palmDirection) - convert(glm::vec3(0));
        const auto current = position(finger->second) - position(hand->second);
        if (glm::length(direction) > 0.001f && glm::length(current) > 0.001f) {
            const auto blended = glm::mix(glm::normalize(current), glm::normalize(direction), std::clamp(weight, 0.f, 1.f));
            if (glm::length(blended) > 0.001f) align(hand->second, finger->second, position(hand->second) + blended);
        }
    }
}

bool SkeletalAnimation::LoadModel(const std::string& path)
{
    Reset();
    static std::unordered_map<std::string, std::weak_ptr<Data::Asset>> cache;
    if (auto shared = cache[path].lock())
    {
        data->asset = std::move(shared);
        for (const auto& node : data->asset->nodes) data->pose.push_back(node.bind);
        data->RefreshPose();
        return true;
    }
    Assimp::Importer importer;
    Configure(importer);
    const auto* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals |
        aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_RemoveComponent);
    if (!scene || !scene->mRootNode || !scene->HasMeshes())
    {
        data->error = "Model load failed: " + path + ": " + importer.GetErrorString();
        return false;
    }
    data->asset->inverseRoot = scene->mRootNode->mTransformation;
    data->asset->inverseRoot.Inverse();
    std::vector<std::pair<const aiNode*, size_t>> meshNodes;
    bool valid = true;
    std::function<void(const aiNode*, int)> visit = [&](const aiNode* node, int parent)
    {
        const size_t id = data->asset->nodes.size();
        Data::Node entry{ NodeName(node->mName), parent, {} };
        node->mTransformation.Decompose(entry.bind.scale, entry.bind.rotation, entry.bind.position);
        if (!data->asset->nodeIds.emplace(entry.name, id).second) valid = false;
        data->asset->nodes.push_back(entry);
        data->pose.push_back(entry.bind);
        meshNodes.emplace_back(node, id);
        for (unsigned i = 0; i < node->mNumChildren; ++i) visit(node->mChildren[i], static_cast<int>(id));
    };
    visit(scene->mRootNode, -1);
    if (!valid) { data->error = "Ambiguous skeleton node names"; return false; }
    for (const auto& instance : meshNodes)
    {
        for (unsigned m = 0; m < instance.first->mNumMeshes; ++m)
        {
            const auto* mesh = scene->mMeshes[instance.first->mMeshes[m]];
            const size_t start = data->asset->sources.size();
            for (unsigned v = 0; v < mesh->mNumVertices; ++v)
            {
                AnimatedVertex vertex{ mesh->mVertices[v], mesh->mNormals[v], {} };
                if (mesh->HasTextureCoords(0)) vertex.uv = mesh->mTextureCoords[0][v];
                data->asset->sources.push_back({ vertex, instance.second, {} });
            }
            for (unsigned b = 0; b < mesh->mNumBones; ++b)
            {
                const auto* bone = mesh->mBones[b];
                const auto found = data->asset->nodeIds.find(NodeName(bone->mName));
                if (found == data->asset->nodeIds.end()) { data->error = "Bone node missing"; return false; }
                const size_t boneId = data->asset->bones.size();
                data->asset->bones.push_back({ found->second, bone->mOffsetMatrix });
                for (unsigned w = 0; w < bone->mNumWeights; ++w)
                {
                    const auto& weight = bone->mWeights[w];
                    if (weight.mVertexId < mesh->mNumVertices && std::isfinite(weight.mWeight) && weight.mWeight > 0)
                        data->asset->sources[start + weight.mVertexId].weights.push_back({ boneId, weight.mWeight });
                }
            }
            for (unsigned f = 0; f < mesh->mNumFaces; ++f)
            {
                const auto& face = mesh->mFaces[f];
                if (face.mNumIndices != 3) continue;
                for (unsigned i = 0; i < 3; ++i) data->asset->indices.push_back(static_cast<unsigned int>(start + face.mIndices[i]));
            }
        }
    }
    if (data->asset->bones.empty() || data->asset->indices.empty()) { data->error = "Model has no skinned triangles"; return false; }
    const size_t count = data->asset->bones.size() + data->asset->nodes.size();
    data->asset->boundsMin.assign(count, glm::vec3((std::numeric_limits<float>::max)()));
    data->asset->boundsMax.assign(count, glm::vec3(-(std::numeric_limits<float>::max)()));
    for (const auto& source : data->asset->sources)
    {
        const glm::vec3 point(source.vertex.position.x, source.vertex.position.y, source.vertex.position.z);
        auto include = [&](size_t bone) {
            data->asset->boundsMin[bone] = (glm::min)(data->asset->boundsMin[bone], point);
            data->asset->boundsMax[bone] = (glm::max)(data->asset->boundsMax[bone], point);
        };
        if (source.weights.empty()) include(data->asset->bones.size() + source.node);
        else for (const auto& weight : source.weights) include(weight.bone);
    }
    data->RefreshPose();
    cache[path] = data->asset;
    return true;
}

bool SkeletalAnimation::LoadAppearance(const std::string& path)
{
    if (data->asset->nodes.empty() || !data->asset->clips.empty()) {
        data->error = "Load character appearance after the rig and before animation clips"; return false;
    }
    static std::unordered_map<std::string, std::weak_ptr<Data::Asset>> cache;
    if (auto shared = cache[path].lock()) {
        data->asset = std::move(shared);
        data->pose.clear();
        for (const auto& node : data->asset->nodes) data->pose.push_back(node.bind);
        data->RefreshPose();
        return true;
    }
    std::ifstream in(path, std::ios::binary);
    auto read = [&](auto& value) { in.read(reinterpret_cast<char*>(&value), sizeof(value)); };
    char magic[8]{};
    in.read(magic, 8);
    std::uint32_t jointCount = 0, vertexCount = 0, indexCount = 0;
    read(jointCount); read(vertexCount); read(indexCount);
    auto fail = [&]() { data->error = "Invalid character appearance: " + path; return false; };
    const bool legacyGarments = std::string(magic, 8) == "TKCHAR02";
    const bool garmentData = legacyGarments || std::string(magic, 8) == "TKCHAR03";
    if (!in || (!garmentData && std::string(magic, 8) != "TKCHAR01") || jointCount == 0 || jointCount > 256 ||
        vertexCount == 0 || vertexCount > 1000000 || indexCount == 0 || indexCount > 6000000 || indexCount % 3) return fail();
    auto next = std::make_shared<Data::Asset>(*data->asset);
    std::unordered_map<size_t, aiVector3D> positions;
    std::vector<size_t> joints;
    auto root = next->inverseRoot;
    root.Inverse();
    for (std::uint32_t i = 0; i < jointCount; ++i) {
        std::uint32_t length = 0;
        read(length);
        if (!in || length == 0 || length > 128) return fail();
        std::string name(length, '\0');
        in.read(name.data(), length);
        aiVector3D position;
        read(position.x); read(position.y); read(position.z);
        const auto node = next->nodeIds.find(name);
        if (!in || node == next->nodeIds.end() || !std::isfinite(position.x) ||
            !std::isfinite(position.y) || !std::isfinite(position.z)) return fail();
        joints.push_back(node->second);
        positions.emplace(node->second, root * position);
    }
    // Retain the reference rig's local axes, while fitting joint positions to the PMX body.
    std::vector<aiMatrix4x4> globals(next->nodes.size());
    for (size_t i = 0; i < next->nodes.size(); ++i) {
        auto& node = next->nodes[i];
        next->animationBindPositions.push_back(node.bind.position);
        const aiMatrix4x4 parent = node.parent < 0 ? aiMatrix4x4() : globals[node.parent];
        if (const auto target = positions.find(i); target != positions.end()) {
            auto inverse = parent;
            inverse.Inverse();
            node.bind.position = inverse * target->second;
        }
        globals[i] = parent * aiMatrix4x4(node.bind.scale, node.bind.rotation, node.bind.position);
    }
    next->bones.clear(); next->sources.clear(); next->indices.clear();
    for (const auto node : joints) {
        auto inverse = globals[node];
        inverse.Inverse();
        next->bones.push_back({node, inverse * root});
    }
    next->sources.reserve(vertexCount);
    for (std::uint32_t i = 0; i < vertexCount; ++i) {
        float values[8]{};
        std::uint32_t bone[4]{};
        float weights[4]{};
        for (auto& v : values) read(v);
        for (auto& b : bone) read(b);
        for (auto& w : weights) read(w);
        std::uint32_t garment = 0;
        if (garmentData) read(garment);
        if (garment > 5) return fail();
        if (legacyGarments && (garment == 3 || garment == 4)) garment = 0;
        if (!in) return fail();
        for (auto v : values) if (!std::isfinite(v)) return fail();
        Data::Source source{{{values[0], values[1], values[2]}, {values[3], values[4], values[5]},
            {values[6], values[7], 0}}, joints.front(), {}};
        source.garment = static_cast<int>(garment);
        float total = 0;
        for (int j = 0; j < 4; ++j) {
            if (bone[j] >= jointCount || !std::isfinite(weights[j]) || weights[j] < 0) return fail();
            if (weights[j] > 0) source.weights.push_back({bone[j], weights[j]});
            total += weights[j];
        }
        if (std::abs(total - 1.f) > 0.001f) return fail();
        next->sources.push_back(std::move(source));
    }
    for (std::uint32_t i = 0; i < indexCount; ++i) {
        std::uint32_t index = 0; read(index);
        if (!in || index >= vertexCount) return fail();
        next->indices.push_back(index);
    }
    if (in.peek() != std::char_traits<char>::eof()) return fail();
    const size_t count = next->bones.size() + next->nodes.size();
    next->boundsMin.assign(count, glm::vec3((std::numeric_limits<float>::max)()));
    next->boundsMax.assign(count, glm::vec3(-(std::numeric_limits<float>::max)()));
    for (const auto& source : next->sources) {
        const glm::vec3 p(source.vertex.position.x, source.vertex.position.y, source.vertex.position.z);
        for (const auto& w : source.weights) {
            next->boundsMin[w.bone] = (glm::min)(next->boundsMin[w.bone], p);
            next->boundsMax[w.bone] = (glm::max)(next->boundsMax[w.bone], p);
        }
    }
    data->asset = next;
    data->pose.clear();
    for (const auto& node : next->nodes) data->pose.push_back(node.bind);
    data->RefreshPose();
    cache[path] = next;
    return true;
}

bool SkeletalAnimation::LoadClip(const std::string& name, const std::string& path)
{
    if (data->asset->nodes.empty()) { data->error = "Load the model before clips"; return false; }
    const auto cached = data->asset->clipPaths.find(name);
    if (cached != data->asset->clipPaths.end() && cached->second == path) return true;
    if (cached != data->asset->clipPaths.end())
    { data->error = "Clip name already loaded from a different path"; return false; }
    Assimp::Importer importer;
    Configure(importer);
    const auto* scene = importer.ReadFile(path, aiProcess_RemoveComponent);
    if (!scene || !scene->HasAnimations())
    {
        data->error = "Animation load failed: " + path + ": " + importer.GetErrorString();
        return false;
    }
    // FBX files may contain an empty authoring take before the actual Mixamo take.
    const aiAnimation* animation = nullptr;
    size_t mostKeys = 0;
    for (unsigned a = 0; a < scene->mNumAnimations; ++a)
    {
        const auto* candidate = scene->mAnimations[a];
        size_t keys = 0;
        for (unsigned c = 0; c < candidate->mNumChannels; ++c)
            keys += candidate->mChannels[c]->mNumRotationKeys + candidate->mChannels[c]->mNumPositionKeys;
        if (keys > mostKeys && candidate->mDuration > 0) { animation = candidate; mostKeys = keys; }
    }
    if (!animation) { data->error = "No playable animation take"; return false; }
    Data::Clip clip;
    clip.duration = animation->mDuration;
    clip.rate = animation->mTicksPerSecond > 0 ? animation->mTicksPerSecond : 25;
    if (!std::isfinite(clip.duration) || clip.duration <= 0 || !std::isfinite(clip.rate))
    { data->error = "Invalid animation duration"; return false; }
    for (unsigned i = 0; i < animation->mNumChannels; ++i)
    {
        const auto* channel = animation->mChannels[i];
        const auto found = data->asset->nodeIds.find(NodeName(channel->mNodeName));
        if (found == data->asset->nodeIds.end()) continue;
        Data::Track track;
        if (channel->mNumPositionKeys) {
            track.positions.assign(channel->mPositionKeys, channel->mPositionKeys + channel->mNumPositionKeys);
            if (!data->asset->animationBindPositions.empty()) {
                const auto offset = data->asset->nodes[found->second].bind.position - data->asset->animationBindPositions[found->second];
                for (auto& key : track.positions) key.mValue += offset;
            }
        }
        if (channel->mNumRotationKeys) track.rotations.assign(channel->mRotationKeys, channel->mRotationKeys + channel->mNumRotationKeys);
        if (channel->mNumScalingKeys) track.scales.assign(channel->mScalingKeys, channel->mScalingKeys + channel->mNumScalingKeys);
        clip.tracks.emplace(found->second, std::move(track));
    }
    // End joints often have no keys; they inherit their animated parent's transform.
    std::unordered_set<size_t> boneNodes;
    size_t matched = 0;
    for (const auto& bone : data->asset->bones) boneNodes.insert(bone.node);
    for (const auto node : boneNodes) if (clip.tracks.count(node)) ++matched;
    if (matched * 2 < boneNodes.size())
    { data->error = "Animation skeleton does not match the model"; return false; }
    data->asset->clips[name] = std::move(clip);
    data->asset->clipPaths[name] = path;
    data->error.clear();
    return true;
}

void SkeletalAnimation::Update(float dt, const std::string& name, bool playing, bool skin)
{
    const auto found = data->asset->clips.find(name);
    if (found == data->asset->clips.end() || !std::isfinite(dt) || dt < 0) return;
    const auto& clip = found->second;
    if (data->selected != name || data->playing != playing)
    {
        data->transition = data->pose;
        data->selected = name;
        data->playing = playing;
        data->time = 0;
        data->idleTime = 0;
        data->blendTime = 0;
    }
    const float elapsed = (std::min)(dt, 1.0f);
    if (playing) data->time = std::fmod(data->time + elapsed * clip.rate, clip.duration);
    else data->idleTime = std::fmod(data->idleTime + elapsed, 3.5);
    data->blendTime = (std::min)(data->blendTime + elapsed, 0.15f);
    for (size_t i = 0; i < data->asset->nodes.size(); ++i)
    {
        const auto& node = data->asset->nodes[i];
        Pose target = node.bind;
        const auto track = clip.tracks.find(i);
        if (track != clip.tracks.end())
        {
            target.position = Sample(track->second.positions, data->time, target.position, Lerp);
            target.scale = Sample(track->second.scales, data->time, target.scale, Lerp);
            target.rotation = Sample(track->second.rotations, data->time, target.rotation, Slerp);
            // Physics owns horizontal translation. Preserve vertical bob and crouch height.
            if (node.parent < 0 || node.name == "Hips")
            {
                target.position.x = node.bind.position.x;
                target.position.z = node.bind.position.z;
            }
        }
        if (!playing)
        {
            // Stand with planted legs from the bind pose and relaxed arms from the walk pose.
            // Crouch idle retains the crouched clip's first-frame lower-body pose.
            const bool lowerBody = node.name == "Hips" ||
                node.name.find("Leg") != std::string::npos ||
                node.name.find("Foot") != std::string::npos ||
                node.name.find("Toe") != std::string::npos;
            if (name != "crouch" && lowerBody) target = node.bind;
            const float breath = std::sin(static_cast<float>(data->idleTime * 6.283185307 / 3.5));
            if (node.name == "Spine2")
            {
                target.scale.y *= 1.0f + 0.004f * breath;
                target.scale.z *= 1.0f + 0.006f * breath;
                target.rotation = target.rotation * aiQuaternion(aiVector3D(1, 0, 0), 0.008f * breath);
                target.rotation.Normalize();
            }
        }
        data->pose[i] = data->transition.empty() ? target :
            Blend(data->transition[i], target, data->blendTime / 0.15f);
    }
    if (skin) data->Skin();
}

void SkeletalAnimation::RefreshPose() { data->RefreshPose(); }
const std::vector<glm::mat4>& SkeletalAnimation::Palette() const { return data->gpuPalette; }
glm::vec3 SkeletalAnimation::BoundsMin() const { return data->boundsMin; }
glm::vec3 SkeletalAnimation::BoundsMax() const { return data->boundsMax; }
void SkeletalAnimation::CreateMesh(Mesh& mesh) const
{
    std::vector<SkinVertex> vertices;
    std::vector<glm::vec2> weights;
    vertices.reserve(data->asset->sources.size());
    for (const auto& source : data->asset->sources)
    {
        SkinVertex vertex;
        vertex.pos = {source.vertex.position.x, source.vertex.position.y, source.vertex.position.z};
        vertex.normal = {source.vertex.normal.x, source.vertex.normal.y, source.vertex.normal.z};
        vertex.uv = {source.vertex.uv.x, source.vertex.uv.y};
        vertex.garment = source.garment;
        vertex.influences.x = static_cast<int>(weights.size());
        if (source.weights.empty())
            weights.emplace_back(static_cast<float>(data->asset->bones.size() + source.node), 1.0f);
        else
        {
            float sum = 0;
            for (const auto& weight : source.weights) sum += weight.value;
            for (const auto& weight : source.weights)
                weights.emplace_back(static_cast<float>(weight.bone), weight.value / sum);
        }
        vertex.influences.y = static_cast<int>(weights.size()) - vertex.influences.x;
        vertices.push_back(vertex);
    }
    mesh.CreateSkinned(vertices, data->asset->indices, weights);
}
