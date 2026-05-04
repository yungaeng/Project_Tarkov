#include "ModelLoader.h"
#include "Logger.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

Mesh ModelLoader::LoadFBX(const std::string& path)
{
    Assimp::Importer importer;

    const aiScene* scene =
        importer.ReadFile(
            path,
            aiProcess_Triangulate |
            aiProcess_GenNormals |
            aiProcess_FlipUVs);

    if (!scene)
    {
        Logger::Error(
            importer.GetErrorString());
    }

    aiMesh* mesh =
        scene->mMeshes[0];

    std::vector<Vertex> verts;

    for (unsigned m = 0;
        m < scene->mNumMeshes;
        m++)
    {
        aiMesh* mesh =
            scene->mMeshes[m];

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

    aiMaterial* mat =
        scene->mMaterials[
            mesh->mMaterialIndex];

    aiString texPath;

    if (mat->GetTexture(
        aiTextureType_DIFFUSE,
        0,
        &texPath) == AI_SUCCESS)
    {
        Logger::Info(
            texPath.C_Str());
    }

    Mesh result;
    result.Create(verts);

    return result;
}