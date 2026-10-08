#include "ModelLoader.h"
#include "../Core/Logger.h"
#include <filesystem>
#include <vector>
#include <utility>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

LoadedModel ModelLoader::LoadFBX(const std::string& path)
{
    LoadedModel result;

    Assimp::Importer importer;

    const aiScene* scene =
        importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs);

    if (!scene || !scene->HasMeshes())
    {
        Logger::Error(
            scene ? "No meshes in model" :
            importer.GetErrorString());
        return result;
    }

    std::vector<Vertex> verts;
    aiMesh* lastMesh = nullptr;

    for (unsigned m = 0;
        m < scene->mNumMeshes;
        m++)
    {
        aiMesh* mesh =
            scene->mMeshes[m];

        lastMesh = mesh;

        for (unsigned i = 0;
            i < mesh->mNumFaces;
            i++)
        {
            aiFace face =
                mesh->mFaces[i];

            for (unsigned j = 0;
                j < face.mNumIndices;
                j++)
            {
                unsigned idx =
                    face.mIndices[j];

                Vertex v;

                v.pos.x =
                    mesh->mVertices[idx].x;

                v.pos.y =
                    mesh->mVertices[idx].y;

                v.pos.z =
                    mesh->mVertices[idx].z;

                if (mesh->HasNormals())
                {
                    v.normal.x =
                        mesh->mNormals[idx].x;

                    v.normal.y =
                        mesh->mNormals[idx].y;

                    v.normal.z =
                        mesh->mNormals[idx].z;
                }
                else
                {
                    v.normal = { 0, 1, 0 };
                }

                if (mesh->HasTextureCoords(0))
                {
                    v.uv.x =
                        mesh->mTextureCoords[0][idx].x;

                    v.uv.y =
                        mesh->mTextureCoords[0][idx].y;
                }
                else
                {
                    v.uv = { 0,0 };
                }

                verts.push_back(v);
            }
        }
    }

    if (lastMesh &&
        lastMesh->mMaterialIndex <
        scene->mNumMaterials)
    {
        aiMaterial* mat =
            scene->mMaterials[
                lastMesh->mMaterialIndex];

        aiString texPath;

        if (mat->GetTexture(
            aiTextureType_DIFFUSE,
            0,
            &texPath) == AI_SUCCESS)
        {
            std::filesystem::path modelDir =
                std::filesystem::path(path).parent_path();

            result.diffuseTexturePath =
                (modelDir / texPath.C_Str())
                .string();

            Logger::Info(
                result.diffuseTexturePath);
        }
    }

    result.mesh.Create(verts);

    return result;
}
