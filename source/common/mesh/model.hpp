#pragma once

#include "mesh.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <glm/glm.hpp>
#include <json/json.hpp>

namespace our {

    inline glm::mat4 AssimpMatToGlm(const aiMatrix4x4& from) {
        glm::mat4 to;
        to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
        to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
        to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
        to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
        return to;
    }

    struct BoneInfo {
        int id;
        glm::mat4 offset;
    };

    struct AssimpNodeData {
        glm::mat4 transformation;
        std::string name;
        int childrenCount;
        std::vector<AssimpNodeData> children;
    };

    class Model {
    public:
        Mesh* getMesh() { return mesh; }
        auto& getBoneInfoMap() { return boneInfoMap; }
        int& getBoneCount() { return boneCounter; }
        
        // Load model and animation. If preTransform is true, all node transforms
        // are baked into vertices (for static multi-part models like ships).
        bool load(const std::string& path, bool preTransform = false);
        
        // Getting animation from Assimp (keeping importer alive is easier for basic assignments)
        const aiScene* getScene() const { return scene; }
        Assimp::Importer importer;

    private:
        Mesh* mesh = nullptr;
        std::map<std::string, BoneInfo> boneInfoMap;
        int boneCounter = 0;
        const aiScene* scene = nullptr;

        void processNode(aiNode *node, const aiScene *n_scene, std::vector<our::Vertex>& vertices, std::vector<unsigned int>& indices, const glm::mat4& parentTransform = glm::mat4(1.0f));
        void processMesh(aiMesh *aimesh, const aiScene *n_scene, std::vector<our::Vertex>& vertices, std::vector<unsigned int>& indices, const glm::mat4& nodeTransform = glm::mat4(1.0f));
        void setVertexBoneDataToDefault(our::Vertex& vertex);
        void setVertexBoneData(our::Vertex& vertex, int boneID, float weight);
        void extractBoneWeightForVertices(std::vector<our::Vertex>& vertices, aiMesh* aimesh, int baseVertex);
    };

    // Global registry for animated models so we can easily fetch them in AssetLoader
    class ModelLoader {
    public:
        static std::unordered_map<std::string, Model*> models;
        static void deserialize(const nlohmann::json& data);
        static void clear();
    };

}
