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
    struct Source { AnimatedVertex vertex; size_t node; std::vector<Weight> weights; };
    struct Track
    {
        std::vector<aiVectorKey> positions, scales;
        std::vector<aiQuatKey> rotations;
    };
    struct Clip { double duration = 0, rate = 25; std::unordered_map<size_t, Track> tracks; };
    std::vector<Node> nodes;
    std::unordered_map<std::string, size_t> nodeIds;
    std::vector<Bone> bones;
    std::vector<Source> sources;
    std::vector<size_t> indices;
    std::vector<AnimatedVertex> output;
    std::vector<Pose> pose, transition;
    std::unordered_map<std::string, Clip> clips;
    aiMatrix4x4 inverseRoot;
    std::string error, selected;
    double time = 0;
    double idleTime = 0;
    float blendTime = 0.15f;
    bool playing = false;

    void Skin()
    {
        std::vector<aiMatrix4x4> globals(nodes.size());
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const auto& p = pose[i];
            const aiMatrix4x4 local(p.scale, p.rotation, p.position);
            globals[i] = nodes[i].parent < 0 ? local : globals[nodes[i].parent] * local;
        }
        std::vector<aiMatrix4x4> palette;
        std::vector<aiMatrix3x3> normals;
        for (const auto& bone : bones)
        {
            palette.push_back(inverseRoot * globals[bone.node] * bone.offset);
            aiMatrix3x3 normal(palette.back());
            normals.push_back(normal.Inverse().Transpose());
        }
        std::vector<AnimatedVertex> vertices;
        vertices.reserve(sources.size());
        for (const auto& source : sources)
        {
            AnimatedVertex vertex = source.vertex;
            if (source.weights.empty())
            {
                const aiMatrix4x4 rigid = inverseRoot * globals[source.node];
                vertex.position = rigid * source.vertex.position;
                aiMatrix3x3 normal(rigid);
                vertex.normal = normal.Inverse().Transpose() * source.vertex.normal;
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
            vertices.push_back(vertex);
        }
        output.resize(indices.size());
        for (size_t i = 0; i < indices.size(); ++i) output[i] = vertices[indices[i]];
    }
};

SkeletalAnimation::SkeletalAnimation() : data(std::make_unique<Data>()) {}
SkeletalAnimation::~SkeletalAnimation() = default;
void SkeletalAnimation::Reset() { data = std::make_unique<Data>(); }
const std::string& SkeletalAnimation::Error() const { return data->error; }
size_t SkeletalAnimation::BoneCount() const { return data->bones.size(); }
const std::vector<AnimatedVertex>& SkeletalAnimation::Vertices() const { return data->output; }

bool SkeletalAnimation::LoadModel(const std::string& path)
{
    Reset();
    Assimp::Importer importer;
    Configure(importer);
    const auto* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals |
        aiProcess_FlipUVs | aiProcess_RemoveComponent);
    if (!scene || !scene->mRootNode || !scene->HasMeshes())
    {
        data->error = "Model load failed: " + path + ": " + importer.GetErrorString();
        return false;
    }
    data->inverseRoot = scene->mRootNode->mTransformation;
    data->inverseRoot.Inverse();
    std::vector<std::pair<const aiNode*, size_t>> meshNodes;
    bool valid = true;
    std::function<void(const aiNode*, int)> visit = [&](const aiNode* node, int parent)
    {
        const size_t id = data->nodes.size();
        Data::Node entry{ NodeName(node->mName), parent, {} };
        node->mTransformation.Decompose(entry.bind.scale, entry.bind.rotation, entry.bind.position);
        if (!data->nodeIds.emplace(entry.name, id).second) valid = false;
        data->nodes.push_back(entry);
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
            const size_t start = data->sources.size();
            for (unsigned v = 0; v < mesh->mNumVertices; ++v)
            {
                AnimatedVertex vertex{ mesh->mVertices[v], mesh->mNormals[v], {} };
                if (mesh->HasTextureCoords(0)) vertex.uv = mesh->mTextureCoords[0][v];
                data->sources.push_back({ vertex, instance.second, {} });
            }
            for (unsigned b = 0; b < mesh->mNumBones; ++b)
            {
                const auto* bone = mesh->mBones[b];
                const auto found = data->nodeIds.find(NodeName(bone->mName));
                if (found == data->nodeIds.end()) { data->error = "Bone node missing"; return false; }
                const size_t boneId = data->bones.size();
                data->bones.push_back({ found->second, bone->mOffsetMatrix });
                for (unsigned w = 0; w < bone->mNumWeights; ++w)
                {
                    const auto& weight = bone->mWeights[w];
                    if (weight.mVertexId < mesh->mNumVertices && std::isfinite(weight.mWeight) && weight.mWeight > 0)
                        data->sources[start + weight.mVertexId].weights.push_back({ boneId, weight.mWeight });
                }
            }
            for (unsigned f = 0; f < mesh->mNumFaces; ++f)
            {
                const auto& face = mesh->mFaces[f];
                if (face.mNumIndices != 3) continue;
                for (unsigned i = 0; i < 3; ++i) data->indices.push_back(start + face.mIndices[i]);
            }
        }
    }
    if (data->bones.empty() || data->indices.empty()) { data->error = "Model has no skinned triangles"; return false; }
    data->Skin();
    return true;
}

bool SkeletalAnimation::LoadClip(const std::string& name, const std::string& path)
{
    if (data->nodes.empty()) { data->error = "Load the model before clips"; return false; }
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
        const auto found = data->nodeIds.find(NodeName(channel->mNodeName));
        if (found == data->nodeIds.end()) continue;
        Data::Track track;
        if (channel->mNumPositionKeys) track.positions.assign(channel->mPositionKeys, channel->mPositionKeys + channel->mNumPositionKeys);
        if (channel->mNumRotationKeys) track.rotations.assign(channel->mRotationKeys, channel->mRotationKeys + channel->mNumRotationKeys);
        if (channel->mNumScalingKeys) track.scales.assign(channel->mScalingKeys, channel->mScalingKeys + channel->mNumScalingKeys);
        clip.tracks.emplace(found->second, std::move(track));
    }
    // End joints often have no keys; they inherit their animated parent's transform.
    std::unordered_set<size_t> boneNodes;
    size_t matched = 0;
    for (const auto& bone : data->bones) boneNodes.insert(bone.node);
    for (const auto node : boneNodes) if (clip.tracks.count(node)) ++matched;
    if (matched * 2 < boneNodes.size())
    { data->error = "Animation skeleton does not match the model"; return false; }
    data->clips[name] = std::move(clip);
    data->error.clear();
    return true;
}

void SkeletalAnimation::Update(float dt, const std::string& name, bool playing)
{
    const auto found = data->clips.find(name);
    if (found == data->clips.end() || !std::isfinite(dt) || dt < 0) return;
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
    for (size_t i = 0; i < data->nodes.size(); ++i)
    {
        const auto& node = data->nodes[i];
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
    data->Skin();
}
