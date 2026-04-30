#include "model.hpp"
#include <iostream>
#include <glm/gtc/type_ptr.hpp>

namespace our
{

    std::unordered_map<std::string, Model *> ModelLoader::models;

    void ModelLoader::deserialize(const nlohmann::json &data)
    {
        if (data.is_object())
        {
            for (auto &[name, desc] : data.items())
            {
                std::string path = desc.get<std::string>();
                Model *model = new Model();
                if (model->load(path))
                {
                    models[name] = model;
                }
                else
                {
                    delete model;
                }
            }
        }
    }

    void ModelLoader::clear()
    {
        for (auto &pair : models)
        {
            delete pair.second;
        }
        models.clear();
    }

    bool Model::load(const std::string &path, bool preTransform)
    {
        scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            std::cerr << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
            return false;
        }

        std::vector<our::Vertex> globalVertices;
        std::vector<unsigned int> globalIndices;

        processNode(scene->mRootNode, scene, globalVertices, globalIndices);

        mesh = new Mesh(globalVertices, globalIndices);
        return true;
    }

    void Model::processNode(aiNode *node, const aiScene *n_scene, std::vector<our::Vertex> &vertices, std::vector<unsigned int> &indices, const glm::mat4 &parentTransform)
    {
        // Accumulate this node's transform with its parent's
        glm::mat4 nodeTransform = parentTransform * AssimpMatToGlm(node->mTransformation);

        for (unsigned int i = 0; i < node->mNumMeshes; i++)
        {
            aiMesh *m = n_scene->mMeshes[node->mMeshes[i]];
            processMesh(m, n_scene, vertices, indices, nodeTransform);
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++)
        {
            processNode(node->mChildren[i], n_scene, vertices, indices, nodeTransform);
        }
    }

    void Model::processMesh(aiMesh *aimesh, const aiScene *n_scene, std::vector<our::Vertex> &vertices, std::vector<unsigned int> &indices, const glm::mat4 &nodeTransform)

    {
        int baseVertex = vertices.size();

        // Only apply node transform to vertices if this mesh has NO bones.
        // For boned/animated meshes the bone system handles positioning.
        bool hasBones = (aimesh->mNumBones > 0);
        glm::mat3 normalMatrix = glm::mat3(1.0f);
        if (!hasBones)
        {
            normalMatrix = glm::transpose(glm::inverse(glm::mat3(nodeTransform)));
        }

        for (unsigned int i = 0; i < aimesh->mNumVertices; i++)
        {
            our::Vertex vertex;
            setVertexBoneDataToDefault(vertex);

            glm::vec3 pos(aimesh->mVertices[i].x, aimesh->mVertices[i].y, aimesh->mVertices[i].z);
            glm::vec3 norm(0.0f);
            if (aimesh->HasNormals())
            {
                norm = glm::vec3(aimesh->mNormals[i].x, aimesh->mNormals[i].y, aimesh->mNormals[i].z);
            }

            if (!hasBones)
            {
                // Apply accumulated node transform to position and normal
                pos = glm::vec3(nodeTransform * glm::vec4(pos, 1.0f));
                if (aimesh->HasNormals())
                {
                    norm = glm::normalize(normalMatrix * norm);
                }
            }

            vertex.position = pos;
            vertex.normal = norm;

            if (aimesh->mTextureCoords[0])
            {
                vertex.tex_coord = glm::vec2(aimesh->mTextureCoords[0][i].x, aimesh->mTextureCoords[0][i].y);
            }
            else
            {
                vertex.tex_coord = glm::vec2(0.0f, 0.0f);
            }
            if (aimesh->mColors[0])
            {
                vertex.color = our::Color(aimesh->mColors[0][i].r * 255, aimesh->mColors[0][i].g * 255, aimesh->mColors[0][i].b * 255, aimesh->mColors[0][i].a * 255);
                // Optional fix: if Assimp loaded black colors, force it to white
                if (vertex.color.r == 0 && vertex.color.g == 0 && vertex.color.b == 0)
                {
                    vertex.color = our::Color(255, 255, 255, 255);
                }
            }
            else
            {
                vertex.color = our::Color(255, 255, 255, 255);
            }
            vertices.push_back(vertex);
        }

        for (unsigned int i = 0; i < aimesh->mNumFaces; i++)
        {
            aiFace face = aimesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++)
                indices.push_back(baseVertex + face.mIndices[j]);
        }

        extractBoneWeightForVertices(vertices, aimesh, baseVertex);
    }

    void Model::setVertexBoneDataToDefault(our::Vertex &vertex)
    {
        for (int i = 0; i < MAX_BONE_INFLUENCE; i++)
        {
            vertex.boneIDs[i] = -1;
            vertex.boneWeights[i] = 0.0f;
        }
    }

    void Model::setVertexBoneData(our::Vertex &vertex, int boneID, float weight)
    {
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
        {
            if (vertex.boneIDs[i] < 0)
            {
                vertex.boneWeights[i] = weight;
                vertex.boneIDs[i] = boneID;
                break;
            }
        }
    }

    void Model::extractBoneWeightForVertices(std::vector<our::Vertex> &vertices, aiMesh *aimesh, int baseVertex)
    {
        for (unsigned int boneIndex = 0; boneIndex < aimesh->mNumBones; ++boneIndex)
        {
            int boneID = -1;
            std::string boneName = aimesh->mBones[boneIndex]->mName.C_Str();
            if (boneInfoMap.find(boneName) == boneInfoMap.end())
            {
                BoneInfo newBoneInfo;
                newBoneInfo.id = boneCounter;
                newBoneInfo.offset = AssimpMatToGlm(aimesh->mBones[boneIndex]->mOffsetMatrix);
                boneInfoMap[boneName] = newBoneInfo;
                boneID = boneCounter;
                boneCounter++;
            }
            else
            {
                boneID = boneInfoMap[boneName].id;
            }

            auto weights = aimesh->mBones[boneIndex]->mWeights;
            int numWeights = aimesh->mBones[boneIndex]->mNumWeights;

            for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex)
            {
                int vertexId = weights[weightIndex].mVertexId + baseVertex;
                float weight = weights[weightIndex].mWeight;
                if (vertexId < vertices.size())
                {
                    setVertexBoneData(vertices[vertexId], boneID, weight);
                }
            }
        }
    }

}
