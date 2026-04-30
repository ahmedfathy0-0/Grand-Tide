#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include "../components/light.hpp"
#include "../material/lit-material.hpp"
#include "../material/lit-material.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../components/shark-component.hpp"
#include "../components/octopus-component.hpp"
#include "../components/animator.hpp"
#include "../components/shark-component.hpp"

#include "../components/health.hpp"
#include "../components/inventory.hpp"
#include "../components/resource.hpp"
#include "../components/enemy.hpp"
#include "../components/burn-component.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>
#include <vector>
#include "../mesh/model.hpp"
#include <stb/stb_image.h>
#include <iostream>

namespace our
{

    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config)
    {
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Then we check if there is a sky texture in the configuration
        if (config.contains("sky"))
        {
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));

            // We can draw the sky using the same shader used to draw textured objects (or our procedural one)
            ShaderProgram *skyShader = new ShaderProgram();
            if (config.value<std::string>("sky", "") == "procedural")
            {
                skyShader->attach("assets/shaders/procedural_sky.vert", GL_VERTEX_SHADER);
                skyShader->attach("assets/shaders/procedural_sky.frag", GL_FRAGMENT_SHADER);
            }
            else
            {
                skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
                skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
            }
            skyShader->link();

            // TODO: (Req 10) Pick the correct pipeline state to draw the sky
            //  Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion should we pick?
            //  We will draw the sphere from the inside, so what options should we pick for the face culling.
            PipelineState skyPipelineState{};
            skyPipelineState.depthTesting.enabled = true;
            skyPipelineState.depthTesting.function = GL_LEQUAL;
            skyPipelineState.faceCulling.enabled = true;
            skyPipelineState.faceCulling.culledFace = GL_FRONT;
            skyPipelineState.faceCulling.frontFace = GL_CCW;

            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
            std::string skyTextureFile = config.value<std::string>("sky", "");

            if (skyTextureFile == "procedural" || skyTextureFile.empty())
            {
                this->skyMaterial = new Material();
                this->skyMaterial->shader = skyShader;
                this->skyMaterial->pipelineState = skyPipelineState;
                this->skyMaterial->transparent = false;

                ShaderProgram* waterShader = new ShaderProgram();
                waterShader->attach("assets/shaders/water.vert", GL_VERTEX_SHADER);
                waterShader->attach("assets/shaders/water.frag", GL_FRAGMENT_SHADER);
                waterShader->link();
                this->waterMaterial = new Material();
                this->waterMaterial->shader = waterShader;
                this->waterMaterial->pipelineState = skyPipelineState;
                this->waterMaterial->transparent = false;
            } else {
                Texture2D* skyTexture = texture_utils::loadImage(skyTextureFile, false);

                // Setup a sampler for the sky
                Sampler *skySampler = new Sampler();
                skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
                skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                // Combine all the aforementioned objects (except the mesh) into a material
                TexturedMaterial *texMat = new TexturedMaterial();
                texMat->shader = skyShader;
                texMat->texture = skyTexture;
                texMat->sampler = skySampler;
                texMat->pipelineState = skyPipelineState;
                texMat->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                texMat->alphaThreshold = 1.0f;
                texMat->transparent = false;
                this->skyMaterial = texMat;
            }
        }

        // Then we check if there is a postprocessing shader in the configuration
        if (config.contains("postprocess"))
        {
            // TODO: (Req 11) Create a framebuffer
            glGenFramebuffers(1, &postprocessFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);

            // TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
            //  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
            //  The depth format can be (Depth component with 24 bits).
            colorTarget = texture_utils::empty(GL_RGBA8, windowSize);
            depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT24, windowSize);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);

            // TODO: (Req 11) Unbind the framebuffer just to be safe
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Create a vertex array to use for drawing the texture
            glGenVertexArrays(1, &postProcessVertexArray);

            // Create a sampler to use for sampling the scene texture in the post processing shader
            Sampler *postprocessSampler = new Sampler();
            postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram *postprocessShader = new ShaderProgram();
            postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
            postprocessShader->attach(config.value<std::string>("postprocess", ""), GL_FRAGMENT_SHADER);
            postprocessShader->link();

            // Create a post processing material
            postprocessMaterial = new TexturedMaterial();
            postprocessMaterial->shader = postprocessShader;
            postprocessMaterial->texture = colorTarget;
            postprocessMaterial->sampler = postprocessSampler;
            // The default options are fine but we don't need to interact with the depth buffer
            // so it is more performant to disable the depth mask
            postprocessMaterial->pipelineState.depthMask = false;
        }

        // Initialize shadow map
        glGenFramebuffers(1, &shadowMapFBO);
        shadowMap = texture_utils::empty(GL_DEPTH_COMPONENT24, glm::ivec2(2048, 2048));
        glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap->getOpenGLName(), 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        ShaderProgram* shadowShader = new ShaderProgram();
        shadowShader->attach("assets/shaders/shadow.vert", GL_VERTEX_SHADER);
        shadowShader->attach("assets/shaders/shadow.frag", GL_FRAGMENT_SHADER);
        shadowShader->link();
        shadowMaterial = new Material();
        shadowMaterial->shader = shadowShader;
        shadowMaterial->pipelineState.depthTesting.enabled = true;
        shadowMaterial->pipelineState.faceCulling.enabled = true;
        shadowMaterial->pipelineState.faceCulling.culledFace = GL_FRONT; // prevent peter panning

        // Reflection mapping objects
        glGenFramebuffers(1, &reflectionFBO);
        reflectionColor = texture_utils::empty(GL_RGBA8, windowSize);
        glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reflectionColor->getOpenGLName(), 0);
        GLuint reflectionDepth;
        glGenRenderbuffers(1, &reflectionDepth);
        glBindRenderbuffer(GL_RENDERBUFFER, reflectionDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, windowSize.x, windowSize.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, reflectionDepth);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // --- Radar Skull Marker Initialization ---
        // 1. Load texture using stb_image
        stbi_set_flip_vertically_on_load(false);
        int width, height, channels;
        unsigned char* data = stbi_load("assets/textures/skull-marker.png", &width, &height, &channels, 4);
        if (data) {
            glGenTextures(1, &skullTexture);
            glBindTexture(GL_TEXTURE_2D, skullTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            stbi_image_free(data);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        
        // 2. Create inline shaders
        const char* skullVertSource = R"(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            out vec2 vUV;
            uniform mat4 transform;
            void main() {
                vUV = aPos;
                gl_Position = transform * vec4(aPos, 0.0, 1.0);
            }
        )";
        
        const char* skullFragSource = R"(
            #version 330 core
            uniform sampler2D uSkullTex;
            in vec2 vUV;
            out vec4 fragColor;
            void main() {
                vec4 col = texture(uSkullTex, vUV);
                if (col.a < 0.01) discard;
                fragColor = col;
            }
        )";
        
        GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertShader, 1, &skullVertSource, nullptr);
        glCompileShader(vertShader);
        
        GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragShader, 1, &skullFragSource, nullptr);
        glCompileShader(fragShader);
        
        skullShaderProgram = glCreateProgram();
        glAttachShader(skullShaderProgram, vertShader);
        glAttachShader(skullShaderProgram, fragShader);
        glLinkProgram(skullShaderProgram);
        
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        
        // 3. Create unit quad for skull
        struct SkullVertex { glm::vec2 position; };
        SkullVertex skullVertices[4] = {
            { glm::vec2(0.0f, 0.0f) },
            { glm::vec2(1.0f, 0.0f) },
            { glm::vec2(1.0f, 1.0f) },
            { glm::vec2(0.0f, 1.0f) }
        };
        GLuint skullElements[6] = { 0, 1, 2, 2, 3, 0 };
        
        glGenVertexArrays(1, &skullVAO);
        glBindVertexArray(skullVAO);
        
        glGenBuffers(1, &skullVBO);
        glBindBuffer(GL_ARRAY_BUFFER, skullVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skullVertices), skullVertices, GL_STATIC_DRAW);
        
        GLuint skullEBO;
        glGenBuffers(1, &skullEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skullEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skullElements), skullElements, GL_STATIC_DRAW);
        
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SkullVertex), (void*)0);
        glBindVertexArray(0);

        // --- Wave Ring Shader ---
        const char* waveVertSource = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            uniform mat4 transform;
            uniform vec3 u_waveOrigin;
            uniform float u_innerRadius;
            uniform float u_outerRadius;
            uniform float u_baseY;
            out float v_localU;
            out float v_heightFrac;
            void main() {
                gl_Position = transform * vec4(aPos, 1.0);
                float dist = length(aPos.xz - u_waveOrigin.xz);
                float width = u_outerRadius - u_innerRadius;
                v_localU = (width > 0.001) ? clamp((dist - u_innerRadius) / width, 0.0, 1.0) : 0.0;
                v_heightFrac = clamp((aPos.y - u_baseY) / 0.6, 0.0, 1.0);
            }
        )";
        const char* waveFragSource = R"(
            #version 330 core
            uniform float u_waveT;
            in float v_localU;
            in float v_heightFrac;
            out vec4 fragColor;
            void main() {
                // deep ocean blue, dense, fades as wave expands
                float alpha = (1.0 - u_waveT) * 0.75;
                float edgeFade = smoothstep(0.0, 0.25, v_localU) * smoothstep(1.0, 0.75, v_localU);
                vec4 ringColor = vec4(0.05, 0.35, 0.85, alpha * edgeFade);
                // foam/white highlight at the very top edge
                float foamLine = smoothstep(0.4, 0.6, v_heightFrac);
                ringColor.rgb = mix(ringColor.rgb, vec3(0.7, 0.85, 1.0), foamLine * 0.45);
                fragColor = ringColor;
            }
        )";
        GLuint waveVS = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(waveVS, 1, &waveVertSource, nullptr);
        glCompileShader(waveVS);
        GLuint waveFS = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(waveFS, 1, &waveFragSource, nullptr);
        glCompileShader(waveFS);
        waveShaderProgram = glCreateProgram();
        glAttachShader(waveShaderProgram, waveVS);
        glAttachShader(waveShaderProgram, waveFS);
        glLinkProgram(waveShaderProgram);
        glDeleteShader(waveVS);
        glDeleteShader(waveFS);

        // Wave ring VBO (wall + flat ring = ~520 verts, alloc 1200 for safety)
        glGenVertexArrays(1, &waveVAO);
        glGenBuffers(1, &waveVBO);
        glBindVertexArray(waveVAO);
        glBindBuffer(GL_ARRAY_BUFFER, waveVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 1200, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glBindVertexArray(0);

        // --- Water Screen Distortion ---
        unsigned char* wdata = stbi_load("assets/textures/water-distortion.png", &width, &height, &channels, 4);
        if (wdata) {
            glGenTextures(1, &waterDistortionTexture);
            glBindTexture(GL_TEXTURE_2D, waterDistortionTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, wdata);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            stbi_image_free(wdata);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        const char* waterVertSource = R"(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            out vec2 vUV;
            void main() {
                vUV = aPos;
                gl_Position = vec4(aPos * 2.0 - 1.0, 0.0, 1.0);
            }
        )";
        const char* waterFragSource = R"(
            #version 330 core
            uniform sampler2D uScene;
            uniform sampler2D uWaterTex;
            uniform float uTime;
            uniform float uStrength;
            uniform float uBloodMix;
            in vec2 vUV;
            out vec4 fragColor;
            void main() {
                vec2 uv1 = vUV + vec2(uTime * 0.07, uTime * 0.05);
                vec2 uv2 = vUV * 1.3 + vec2(uTime * -0.04, uTime * 0.06);
                vec2 distort1 = texture(uWaterTex, uv1).rg;
                distort1 = (distort1 * 2.0 - 1.0) * 0.15 * uStrength;
                vec2 distort2 = texture(uWaterTex, uv2).rg;
                distort2 = (distort2 * 2.0 - 1.0) * 0.10 * uStrength;
                vec2 totalDistort = distort1 + distort2;
                vec2 distortedUV = clamp(vUV + totalDistort, 0.0, 1.0);
                vec2 texelSize = 1.0 / textureSize(uScene, 0);
                vec4 color = vec4(0.0);
                for(int x = -1; x <= 1; x++) {
                    for(int y = -1; y <= 1; y++) {
                        color += texture(uScene, distortedUV + vec2(x, y) * texelSize * 2.0 * uStrength);
                    }
                }
                color /= 9.0;
                // Water tint (blue-green)
                color.rgb = mix(color.rgb, color.rgb * vec3(0.7, 0.85, 1.0), 0.3 * uStrength);
                // Blood tint (red) mixed in when wave hit player
                color.rgb = mix(color.rgb, color.rgb * vec3(1.0, 0.3, 0.2), uBloodMix * 0.5 * uStrength);
                fragColor = color;
            }
        )";
        GLuint waterVS = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(waterVS, 1, &waterVertSource, nullptr);
        glCompileShader(waterVS);
        GLuint waterFS = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(waterFS, 1, &waterFragSource, nullptr);
        glCompileShader(waterFS);
        waterScreenShader = glCreateProgram();
        glAttachShader(waterScreenShader, waterVS);
        glAttachShader(waterScreenShader, waterFS);
        glLinkProgram(waterScreenShader);
        glDeleteShader(waterVS);
        glDeleteShader(waterFS);

        float waterQuad[12] = {
            0.0f, 0.0f,  1.0f, 0.0f,  1.0f, 1.0f,
            0.0f, 0.0f,  1.0f, 1.0f,  0.0f, 1.0f
        };
        glGenVertexArrays(1, &waterScreenVAO);
        glGenBuffers(1, &waterScreenVBO);
        glBindVertexArray(waterScreenVAO);
        glBindBuffer(GL_ARRAY_BUFFER, waterScreenVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(waterQuad), waterQuad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    void ForwardRenderer::destroy()
    {
        // Delete all objects related to the sky
        if (skyMaterial)
        {
            delete skySphere;
            delete skyMaterial->shader;
            if (auto texMat = dynamic_cast<TexturedMaterial *>(skyMaterial); texMat)
            {
                delete texMat->texture;
                delete texMat->sampler;
            }
            delete skyMaterial;
        }
        if(waterMaterial){
            delete waterMaterial->shader;
            delete waterMaterial;
        }
        // Delete all objects related to post processing
        if (postprocessMaterial)
        {
            glDeleteFramebuffers(1, &postprocessFrameBuffer);
            glDeleteVertexArrays(1, &postProcessVertexArray);
            delete colorTarget;
            delete depthTarget;
            delete postprocessMaterial->sampler;
            delete postprocessMaterial->shader;
            delete postprocessMaterial;
        }
        
        glDeleteFramebuffers(1, &shadowMapFBO);
        delete shadowMap;
        delete shadowMaterial->shader;
        delete shadowMaterial;

        // Cleanup radar skull
        if (skullTexture) glDeleteTextures(1, &skullTexture);
        if (skullShaderProgram) glDeleteProgram(skullShaderProgram);
        if (skullVAO) glDeleteVertexArrays(1, &skullVAO);
        if (skullVBO) glDeleteBuffers(1, &skullVBO);

        // Cleanup wave ring
        if (waveShaderProgram) glDeleteProgram(waveShaderProgram);
        if (waveVAO) glDeleteVertexArrays(1, &waveVAO);
        if (waveVBO) glDeleteBuffers(1, &waveVBO);

        // Cleanup water screen distortion
        if (waterDistortionTexture) glDeleteTextures(1, &waterDistortionTexture);
        if (waterScreenShader) glDeleteProgram(waterScreenShader);
        if (waterScreenVAO) glDeleteVertexArrays(1, &waterScreenVAO);
        if (waterScreenVBO) glDeleteBuffers(1, &waterScreenVBO);
    }

    void ForwardRenderer::render(World *world)
    {
        // First of all, we search for a camera and for all the mesh renderers
        CameraComponent *camera = nullptr;
        opaqueCommands.clear();
        transparentCommands.clear();
        for (auto entity : world->getEntities())
        {
            // If we hadn't found a camera yet, we look for a camera in this entity
            if (!camera)
                camera = entity->getComponent<CameraComponent>();
            // If this entity has a mesh renderer component
            if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer)
            {
                // We construct a command from it
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;
                command.entity = entity;

                // If it's an animated entity, override the default mesh with the actual FBX rigged mesh
                if (auto animator = entity->getComponent<AnimatorComponent>(); animator && !animator->modelName.empty())
                {
                    if (our::ModelLoader::models.find(animator->modelName) != our::ModelLoader::models.end())
                    {
                        command.mesh = our::ModelLoader::models[animator->modelName]->getMesh();
                    }
                }
                
                // If it's an animated octopus, override the default mesh with the actual FBX rigged mesh
                if (auto octopus = entity->getComponent<OctopusComponent>(); octopus && !octopus->modelName.empty()) {
                    if (our::ModelLoader::models.find(octopus->modelName) != our::ModelLoader::models.end()) {
                        command.mesh = our::ModelLoader::models[octopus->modelName]->getMesh();
                    }
                }

                // if it is transparent, we add it to the transparent commands list
                if (command.material->transparent)
                {
                    transparentCommands.push_back(command);
                }
                else
                {
                    // Otherwise, we add it to the opaque command list
                    opaqueCommands.push_back(command);
                }
            }
        }

        // Gather all light components
        std::vector<std::pair<LightComponent *, glm::mat4>> lights;
        for (auto entity : world->getEntities())
        {
            if (auto light = entity->getComponent<LightComponent>(); light)
            {
                lights.push_back({light, entity->getLocalToWorldMatrix()});
            }
        }

        glm::mat4 cameraLocalToWorld = camera->getOwner()->getLocalToWorldMatrix();
        glm::vec3 cameraPosition = glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, 0, 1));

        // TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward direction
        //  HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one
        glm::vec3 cameraForward = glm::normalize(glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, -1, 0)));
        std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraForward, cameraPosition](const RenderCommand &first, const RenderCommand &second)
                  {
            //TODO: (Req 9) Finish this function
            // HINT: the following return should return true "first" should be drawn before "second". 
            float firstDepth = glm::dot(first.center - cameraPosition, cameraForward);
            float secondDepth = glm::dot(second.center - cameraPosition, cameraForward);
            return firstDepth > secondDepth; });

        // TODO: (Req 9) Get the camera ViewProjection matrix and store it in VP
        glm::ivec2 viewportStart(0, 0);
        glm::ivec2 viewportSize = windowSize;
        glm::mat4 VP = camera->getProjectionMatrix(viewportSize) * camera->getViewMatrix();
        
        // Shadow Pass
        glm::mat4 lightProjection = glm::ortho(-25.0f, 25.0f, -25.0f, 25.0f, 1.0f, 100.0f);
        float shadow_u_time = (float)glfwGetTime();
        float shadow_skyTime = shadow_u_time * 0.05f;
        glm::vec3 sun_dir_shadow = glm::normalize(glm::vec3(0.0f, std::sin(shadow_skyTime), -std::cos(shadow_skyTime)));
        glm::mat4 lightView = glm::lookAt(cameraPosition + sun_dir_shadow * 40.0f, cameraPosition, glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;

        // Unified Sun calculation for all passes
        float u_time = (float)glfwGetTime();
        float dayNightSpeed = 0.05f;
        float skyTime = u_time * dayNightSpeed;
        glm::vec3 sun_dir = glm::normalize(glm::vec3(0.0f, std::sin(skyTime), -std::cos(skyTime)));
        float sun_height = std::max(sun_dir.y, 0.0f);
        glm::vec3 sun_col_day = glm::vec3(1.0f, 0.9f, 0.8f);
        glm::vec3 sun_col_sunset = glm::vec3(1.0f, 0.4f, 0.2f);
        glm::vec3 sun_color = glm::mix(sun_col_sunset, sun_col_day, sun_height);
        float sun_intensity = 3.0f * glm::smoothstep(-0.1f, 0.1f, sun_dir.y);

        glViewport(0, 0, 2048, 2048);
        glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        for (const auto& command : opaqueCommands) {
            shadowMaterial->setup();
            shadowMaterial->shader->set("lightSpaceMatrix", lightSpaceMatrix);
            shadowMaterial->shader->set("M", command.localToWorld);
            if (auto tex_mat = dynamic_cast<TexturedMaterial*>(command.material)) {
                shadowMaterial->shader->set("has_albedo", true);
                glActiveTexture(GL_TEXTURE0);
                tex_mat->texture->bind();
                shadowMaterial->shader->set("albedo", 0);
                shadowMaterial->shader->set("alphaThreshold", tex_mat->alphaThreshold);
            } else {
                shadowMaterial->shader->set("has_albedo", false);
            }
            command.mesh->draw();
            shadowMaterial->teardown();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Reflection Pass (Mirror horizontally)
        glBindFramebuffer(GL_FRAMEBUFFER, reflectionFBO);
        glViewport(0, 0, windowSize.x, windowSize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 reflectionView = camera->getViewMatrix();
        glm::vec3 cameraPositionOriginal = cameraPosition;
        glm::vec3 reflectCamPos = cameraPosition; reflectCamPos.y = -reflectCamPos.y;
        reflectionView = glm::lookAt(reflectCamPos, reflectCamPos + glm::normalize(glm::vec3(camera->getOwner()->getLocalToWorldMatrix() * glm::vec4(0, 0, -1, 0))), glm::vec3(0,1,0));
        
        glm::mat4 reflectionVP = camera->getProjectionMatrix(windowSize) * reflectionView;

        for (const auto& command : opaqueCommands) {
            command.material->setup();
            if (auto lit_material = dynamic_cast<LitMaterial*>(command.material)) {
                lit_material->shader->set("VP", reflectionVP);
                lit_material->shader->set("M", command.localToWorld);
                lit_material->shader->set("M_IT", glm::transpose(glm::inverse(command.localToWorld)));
                lit_material->shader->set("camera_position", reflectCamPos);
            }
            command.material->shader->set("transform", reflectionVP * command.localToWorld);
            command.mesh->draw();
            command.material->teardown();
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);


        //TODO: (Req 9) Set the OpenGL viewport using viewportStart and viewportSize
        glViewport(viewportStart.x, viewportStart.y, viewportSize.x, viewportSize.y);

        // TODO: (Req 9) Set the clear color to black and the clear depth to 1
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f);

        // TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the framebuffer)
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);

        // If there is a postprocess material, bind the framebuffer
        if (postprocessMaterial)
        {
            // TODO: (Req 11) bind the framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);
        }

        // TODO: (Req 9) Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // TODO: (Req 9) Draw all the opaque commands
        //  Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for (const auto &command : opaqueCommands)
        {
            command.material->setup();

            // Set lighting uniforms if applicable
            if (auto lit_material = dynamic_cast<LitMaterial*>(command.material); lit_material != nullptr) {
                lit_material->shader->set("camera_position", cameraPosition);
                lit_material->shader->set("u_time", u_time); // Required for wave animation
                lit_material->shader->set("light_count", (int)lights.size() + 1);

                // Add the sun as the first light
                lit_material->shader->set("lights[0].type", 0);
                lit_material->shader->set("lights[0].color", sun_color);
                lit_material->shader->set("lights[0].intensity", sun_intensity); 
                lit_material->shader->set("lights[0].position", -sun_dir);

                for (size_t i = 0; i < lights.size(); ++i)
                {
                    auto *light = lights[i].first;
                    glm::mat4 lightTrans = lights[i].second;
                    std::string lightStr = "lights[" + std::to_string(i + 1) + "]";
                    
                    int type_val = 0;
                    if (light->type == LightType::POINT) type_val = 1;
                    else if (light->type == LightType::SPOT) type_val = 2;
                    
                    lit_material->shader->set(lightStr + ".type", type_val);
                    lit_material->shader->set(lightStr + ".color", light->color);
                    
                    float active_intensity = light->intensity;
                    if(light->type == LightType::SPOT) {
                        glm::vec3 direction = glm::normalize(glm::vec3(lightTrans * glm::vec4(light->direction, 0.0f)));
                        lit_material->shader->set(lightStr + ".direction", direction);
                        lit_material->shader->set(lightStr + ".cone_angles", light->cone_angles);
                        
                        // Turn player spot on at night only
                        if(light->getOwner()->name == "player" || light->getOwner()->getComponent<CameraComponent>()) {
                           active_intensity *= glm::smoothstep(0.1f, -0.4f, sun_dir.y); 
                        }
                    }
                    lit_material->shader->set(lightStr + ".intensity", active_intensity);
                    
                    if (light->type == LightType::POINT || light->type == LightType::SPOT) {
                        lit_material->shader->set(lightStr + ".position", glm::vec3(lightTrans[3]));
                    } else { 
                        glm::vec3 direction = glm::normalize(glm::vec3(lightTrans * glm::vec4(0, 0, -1, 0)));
                        lit_material->shader->set(lightStr + ".position", direction);
                    }
                }

                lit_material->shader->set("M", command.localToWorld);
                lit_material->shader->set("M_IT", glm::transpose(glm::inverse(command.localToWorld)));
                lit_material->shader->set("VP", VP);
                lit_material->shader->set("lightSpaceMatrix", lightSpaceMatrix);
                glActiveTexture(GL_TEXTURE0 + 5);
                shadowMap->bind();
                lit_material->shader->set("shadow_map", 5);

                // Set bone matrices from AnimatorComponent (single source of truth)
                // If no animator, reset to identity to prevent stale matrices from previous draw calls
                if (auto animator = command.entity->getComponent<AnimatorComponent>(); animator && !animator->finalBonesMatrices.empty()) {
                    for(int b = 0; b < (int)animator->finalBonesMatrices.size() && b < 200; b++) {
                        lit_material->shader->set("finalBonesMatrices[" + std::to_string(b) + "]", animator->finalBonesMatrices[b]);
                    }
                     glm::vec3 tint = lit_material->albedo_tint;
                    if (auto shark = command.entity->getComponent<SharkComponent>(); shark) {
                        if (shark->damageFlashTimer > 0.0f) tint = glm::vec3(1.0f, 0.0f, 0.0f); // Flash Red
                    }
                    lit_material->shader->set("albedo_tint", tint);
                    // Clear any remaining bones beyond what this animator has
                    for(int b = (int)animator->finalBonesMatrices.size(); b < 200; b++) {
                        lit_material->shader->set("finalBonesMatrices[" + std::to_string(b) + "]", glm::mat4(1.0f));
                    }
                } else {
                    // No animator: reset all bone matrices to identity so previous entity's bones don't deform this one
                    for(int b = 0; b < 200; b++) {
                        lit_material->shader->set("finalBonesMatrices[" + std::to_string(b) + "]", glm::mat4(1.0f));
                    }
                }

                // Compute wetness: entities near/below water (Y=0) get wet
                float entityY = command.localToWorld[3][1];
                float current_wetness = glm::smoothstep(2.0f, -1.0f, entityY);
                lit_material->shader->set("u_wetness", current_wetness);

                // Clip plane for octopus: discard fragments below water Y
                if (auto octopus = command.entity->getComponent<OctopusComponent>(); octopus) {
                    glEnable(GL_CLIP_DISTANCE0);
                    lit_material->shader->set("uUseClipPlane", true);
                    lit_material->shader->set("uClipPlaneY", 0.0f);
                } else {
                    lit_material->shader->set("uUseClipPlane", false);
                }
            }

            command.material->shader->set("transform", VP * command.localToWorld);

            // Set time uniform for shaders that need it (fire shader)
            {
                GLint timeLoc = command.material->shader->getUniformLocation("time");
                if (timeLoc >= 0) {
                    command.material->shader->set("time", (float)glfwGetTime());
                }
            }

            command.mesh->draw();
            glDisable(GL_CLIP_DISTANCE0); // safe no-op if not enabled
            command.material->teardown();
        }

        // If there is a sky material, draw the sky
        if (this->skyMaterial)
        {
            // TODO: (Req 10) setup the sky material
            skyMaterial->setup();

            // TODO: (Req 10) Get the camera position
            glm::vec3 cameraPosition = glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, 0, 1));

            // TODO: (Req 10) Create a model matrix for the sy such that it always follows the camera (sky sphere center = camera position)
            glm::mat4 skyModel = glm::translate(glm::mat4(1.0f), cameraPosition);

            // TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
            //  We can acheive the is by multiplying by an extra matrix after the projection but what values should we put in it?
            glm::mat4 alwaysBehindTransform = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f);
            // TODO: (Req 10) set the "transform" uniform
            skyMaterial->shader->set("transform", alwaysBehindTransform * VP * skyModel);
            skyMaterial->shader->set("camera_position", cameraPosition);
            skyMaterial->shader->set("u_time", (float)glfwGetTime());
            
            //TODO: (Req 10) draw the sky sphere
            skySphere->draw();
            skyMaterial->teardown();
        }
        if(this->waterMaterial){
            waterMaterial->setup();
            glm::vec3 cameraPosition = glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, 0, 1));
            glm::mat4 skyModel = glm::translate(glm::mat4(1.0f), cameraPosition);
            glm::mat4 alwaysBehindTransform = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f
            );
            waterMaterial->shader->set("transform", alwaysBehindTransform * VP * skyModel);
            waterMaterial->shader->set("camera_position", cameraPosition);
            waterMaterial->shader->set("u_time", (float)glfwGetTime());
            waterMaterial->shader->set("resolution", glm::vec2((float)windowSize.x, (float)windowSize.y));
            
            glActiveTexture(GL_TEXTURE0 + 6);
            reflectionColor->bind();
            waterMaterial->shader->set("reflection_map", 6);

            // Set lighting uniforms for water
            waterMaterial->shader->set("camera_position", cameraPosition);
            waterMaterial->shader->set("light_count", (int)lights.size() + 1);
            
            // Add the sun as the first light
            waterMaterial->shader->set("lights[0].type", 0);
            waterMaterial->shader->set("lights[0].color", sun_color);
            waterMaterial->shader->set("lights[0].intensity", sun_intensity); 
            waterMaterial->shader->set("lights[0].position", -sun_dir);

            for(size_t i = 0; i < lights.size(); ++i) {
                auto* light = lights[i].first;
                glm::mat4 lightTrans = lights[i].second;
                std::string lightStr = "lights[" + std::to_string(i + 1) + "]";
                
                int type_val = 0;
                if (light->type == LightType::POINT) type_val = 1;
                else if (light->type == LightType::SPOT) type_val = 2;
                
                waterMaterial->shader->set(lightStr + ".type", type_val);
                waterMaterial->shader->set(lightStr + ".color", light->color);
                
                float active_intensity = light->intensity;
                if(light->type == LightType::SPOT) {
                    glm::vec3 direction = glm::normalize(glm::vec3(lightTrans * glm::vec4(light->direction, 0.0f)));
                    waterMaterial->shader->set(lightStr + ".direction", direction);
                    waterMaterial->shader->set(lightStr + ".cone_angles", light->cone_angles);
                    
                    if(light->getOwner()->name == "player" || light->getOwner()->getComponent<CameraComponent>()) {
                        active_intensity *= glm::smoothstep(0.1f, -0.4f, sun_dir.y); 
                    }
                }
                waterMaterial->shader->set(lightStr + ".intensity", active_intensity);
                
                if (light->type == LightType::POINT || light->type == LightType::SPOT) {
                    waterMaterial->shader->set(lightStr + ".position", glm::vec3(lightTrans[3]));
                } else { 
                    glm::vec3 direction = glm::normalize(glm::vec3(lightTrans * glm::vec4(0, 0, -1, 0)));
                    waterMaterial->shader->set(lightStr + ".position", direction);
                }
            }

            skySphere->draw();
            waterMaterial->teardown();
        }
        //TODO: (Req 9) Draw all the transparent commands
        // Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for(const auto& command : transparentCommands){
            command.material->setup();
            
            if (auto lit_material = dynamic_cast<LitMaterial*>(command.material); lit_material != nullptr) {
                lit_material->shader->set("camera_position", cameraPosition);
                lit_material->shader->set("u_time", u_time); // Required for wave animation
                lit_material->shader->set("light_count", (int)lights.size() + 1);

                // Add the sun as the first light
                lit_material->shader->set("lights[0].type", 0);
                lit_material->shader->set("lights[0].color", sun_color);
                lit_material->shader->set("lights[0].intensity", sun_intensity); 
                lit_material->shader->set("lights[0].position", -sun_dir);

                for (size_t i = 0; i < lights.size(); ++i)
                {
                    auto *light = lights[i].first;
                    glm::mat4 lightTrans = lights[i].second;
                    std::string lightStr = "lights[" + std::to_string(i + 1) + "]";
                    
                    int type_val = 0;
                    if (light->type == LightType::POINT) type_val = 1;
                    else if (light->type == LightType::SPOT) type_val = 2;
                    
                    lit_material->shader->set(lightStr + ".type", type_val);
                    lit_material->shader->set(lightStr + ".color", light->color);
                    
                    float active_intensity = light->intensity;
                    if(light->type == LightType::SPOT) {
                        glm::vec3 direction = glm::normalize(glm::vec3(lightTrans * glm::vec4(light->direction, 0.0f)));
                        lit_material->shader->set(lightStr + ".direction", direction);
                        lit_material->shader->set(lightStr + ".cone_angles", light->cone_angles);
                        
                        // Turn player spot on at night only
                        if(light->getOwner()->name == "player" || light->getOwner()->getComponent<CameraComponent>()) {
                           active_intensity *= glm::smoothstep(0.1f, -0.4f, sun_dir.y); 
                        }
                    }
                    lit_material->shader->set(lightStr + ".intensity", active_intensity);
                    
                    if (light->type == LightType::POINT || light->type == LightType::SPOT) {
                        lit_material->shader->set(lightStr + ".position", glm::vec3(lightTrans[3]));
                    } else { 
                        glm::vec3 direction = glm::normalize(glm::vec3(lightTrans * glm::vec4(0, 0, -1, 0)));
                        lit_material->shader->set(lightStr + ".position", direction);
                    }
                }

                lit_material->shader->set("M", command.localToWorld);
                lit_material->shader->set("M_IT", glm::transpose(glm::inverse(command.localToWorld)));
                lit_material->shader->set("VP", VP);
                lit_material->shader->set("lightSpaceMatrix", lightSpaceMatrix);
                glActiveTexture(GL_TEXTURE0 + 5);
                shadowMap->bind();
                lit_material->shader->set("shadow_map", 5);

                // Set bone matrices from AnimatorComponent, reset to identity for non-animated
                if (auto animator = command.entity->getComponent<AnimatorComponent>(); animator && !animator->finalBonesMatrices.empty())
                {
                    for (int b = 0; b < (int)animator->finalBonesMatrices.size() && b < 200; b++)
                    {
                        lit_material->shader->set("finalBonesMatrices[" + std::to_string(b) + "]", animator->finalBonesMatrices[b]);
                    }
                    for (int b = (int)animator->finalBonesMatrices.size(); b < 200; b++)
                    {
                        lit_material->shader->set("finalBonesMatrices[" + std::to_string(b) + "]", glm::mat4(1.0f));
                    }
                }
                else
                {
                    for (int b = 0; b < 200; b++)
                    {
                        lit_material->shader->set("finalBonesMatrices[" + std::to_string(b) + "]", glm::mat4(1.0f));
                    }
                }
            }

            command.material->shader->set("transform", VP * command.localToWorld);

            // Set time uniform for shaders that need it (fire, aoe-indicator, aoe-fire)
            {
                GLint timeLoc = command.material->shader->getUniformLocation("time");
                if (timeLoc >= 0) {
                    command.material->shader->set("time", (float)glfwGetTime());
                }
            }

            // Set burnIntensity uniform for aoe-fire shader from BurnComponent
            {
                GLint burnLoc = command.material->shader->getUniformLocation("burnIntensity");
                if (burnLoc >= 0) {
                    float intensity = 1.0f;
                    if (auto burn = command.entity->getComponent<BurnComponent>(); burn) {
                        intensity = burn->burnIntensity;
                    }
                    command.material->shader->set("burnIntensity", intensity);
                }
            }

            command.mesh->draw();
            command.material->teardown();
        }

        // If there is a postprocess material, apply postprocessing
        if (postprocessMaterial)
        {
            // TODO: (Req 11) Return to the default framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            postprocessMaterial->setup();
            glBindVertexArray(postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            postprocessMaterial->teardown();
        }

        // --- WAVE RING RENDERING ---
        // Render directly to screen after postprocess so it's visible
        {
            OctopusComponent* octopus = nullptr;
            for (auto entity : world->getEntities()) {
                if (auto occ = entity->getComponent<OctopusComponent>(); occ) {
                    octopus = occ;
                    break;
                }
            }
            if (octopus && octopus->waveActive && waveShaderProgram) {
                const int segments = 64;
                float waveH = 0.6f; // thin flat ridge — barely above water
                float centerR = octopus->waveOuterRadius;  // center radius (expands)
                float innerR = centerR - 1.2f;
                float outerR = centerR + 1.2f;
                if (innerR < 0.0f) innerR = 0.0f;
                float baseY = octopus->waveOrigin.y;

                std::vector<glm::vec3> verts;
                verts.reserve(segments * 6);

                // Top ring (triangle strip: inner-top, outer-top)
                for (int i = 0; i <= segments; i++) {
                    float angle = (float)i / (float)segments * 2.0f * glm::pi<float>();
                    float c = std::cos(angle);
                    float s = std::sin(angle);
                    verts.push_back(glm::vec3(
                        octopus->waveOrigin.x + c * innerR,
                        baseY + waveH,
                        octopus->waveOrigin.z + s * innerR));
                    verts.push_back(glm::vec3(
                        octopus->waveOrigin.x + c * outerR,
                        baseY + waveH,
                        octopus->waveOrigin.z + s * outerR));
                }
                // Degenerate triangle to jump to outer wall
                verts.push_back(glm::vec3(
                    octopus->waveOrigin.x + outerR,
                    baseY + waveH,
                    octopus->waveOrigin.z));
                verts.push_back(glm::vec3(
                    octopus->waveOrigin.x + outerR,
                    baseY,
                    octopus->waveOrigin.z));
                // Outer wall (triangle strip: outer-top, outer-bottom)
                for (int i = 0; i <= segments; i++) {
                    float angle = (float)i / (float)segments * 2.0f * glm::pi<float>();
                    float c = std::cos(angle);
                    float s = std::sin(angle);
                    verts.push_back(glm::vec3(
                        octopus->waveOrigin.x + c * outerR,
                        baseY + waveH,
                        octopus->waveOrigin.z + s * outerR));
                    verts.push_back(glm::vec3(
                        octopus->waveOrigin.x + c * outerR,
                        baseY,
                        octopus->waveOrigin.z + s * outerR));
                }

                glBindBuffer(GL_ARRAY_BUFFER, waveVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(glm::vec3), verts.data());

                GLboolean depthMaskState;
                glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskState);
                GLboolean cullFaceState;
                glGetBooleanv(GL_CULL_FACE, &cullFaceState);
                GLboolean depthTestState;
                glGetBooleanv(GL_DEPTH_TEST, &depthTestState);

                glEnable(GL_DEPTH_TEST);
                glDepthFunc(GL_LEQUAL);
                glDisable(GL_CULL_FACE);
                glDepthMask(GL_FALSE);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                glUseProgram(waveShaderProgram);
                glUniformMatrix4fv(glGetUniformLocation(waveShaderProgram, "transform"), 1, GL_FALSE, glm::value_ptr(VP));
                float t = centerR / octopus->waveMaxRadius;
                glUniform1f(glGetUniformLocation(waveShaderProgram, "u_waveT"), t);
                glUniform3f(glGetUniformLocation(waveShaderProgram, "u_waveOrigin"),
                            octopus->waveOrigin.x, octopus->waveOrigin.y, octopus->waveOrigin.z);
                glUniform1f(glGetUniformLocation(waveShaderProgram, "u_innerRadius"), innerR);
                glUniform1f(glGetUniformLocation(waveShaderProgram, "u_outerRadius"), outerR);
                glUniform1f(glGetUniformLocation(waveShaderProgram, "u_baseY"), baseY);

                glBindVertexArray(waveVAO);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, (GLsizei)verts.size());
                glBindVertexArray(0);
                glUseProgram(0);

                glDepthMask(depthMaskState);
                if (depthTestState) glEnable(GL_DEPTH_TEST);
                else glDisable(GL_DEPTH_TEST);
                glDepthFunc(GL_LESS);
                if (cullFaceState) glEnable(GL_CULL_FACE);

                // Once-per-second debug print
                static float lastWavePrint = 0.0f;
                float now = (float)glfwGetTime();
                if (now - lastWavePrint > 1.0f) {
                    std::cout << "[Wave] Rendering at centerRadius=" << centerR << std::endl;
                    lastWavePrint = now;
                }
            }
        }

        // --- WATER SCREEN DISTORTION ---
        // Apply after postprocess, before UI so HUD is not distorted
        {
            OctopusComponent* octopus = nullptr;
            for (auto entity : world->getEntities()) {
                if (auto occ = entity->getComponent<OctopusComponent>(); occ) {
                    octopus = occ;
                    break;
                }
            }
            if (octopus && octopus->screenWaterTimer > 0.0f && waterScreenShader && waterDistortionTexture) {
                float strength = octopus->screenWaterTimer / octopus->screenWaterDuration;

                glUseProgram(waterScreenShader);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, colorTarget->getOpenGLName());
                // Must set filter params — empty() doesn't set them, so default
                // GL_NEAREST_MIPMAP_LINEAR makes texture incomplete (no mipmaps = black)
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glUniform1i(glGetUniformLocation(waterScreenShader, "uScene"), 0);

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, waterDistortionTexture);
                glUniform1i(glGetUniformLocation(waterScreenShader, "uWaterTex"), 1);

                glUniform1f(glGetUniformLocation(waterScreenShader, "uTime"), (float)glfwGetTime());
                glUniform1f(glGetUniformLocation(waterScreenShader, "uStrength"), strength);
                glUniform1f(glGetUniformLocation(waterScreenShader, "uBloodMix"),
                            (octopus->waveHasDealtDamage || octopus->hitRegisteredThisSwing) ? 1.0f : 0.0f);

                glDisable(GL_DEPTH_TEST);
                glDisable(GL_BLEND);
                glDisable(GL_CULL_FACE);

                glBindVertexArray(waterScreenVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);

                glUseProgram(0);
            }
        }

        // Phase 2 UI Pass
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glm::mat4 uiProj = glm::ortho(0.0f, (float)windowSize.x, (float)windowSize.y, 0.0f, -1.0f, 1.0f);

        TintedMaterial *uiMat = dynamic_cast<TintedMaterial *>(AssetLoader<Material>::get("tinted"));
        TintedMaterial *hbMat = dynamic_cast<TintedMaterial *>(AssetLoader<Material>::get("health-bar"));
        TintedMaterial *minimapMat = dynamic_cast<TintedMaterial *>(AssetLoader<Material>::get("minimap"));
        TintedMaterial *tbMat = dynamic_cast<TintedMaterial *>(AssetLoader<Material>::get("toolbar"));
        TexturedMaterial *iconHammerMat = dynamic_cast<TexturedMaterial *>(AssetLoader<Material>::get("icon_hammer"));
        TexturedMaterial *iconNetMat = dynamic_cast<TexturedMaterial *>(AssetLoader<Material>::get("icon_net"));
        TexturedMaterial *iconSpearMat = dynamic_cast<TexturedMaterial *>(AssetLoader<Material>::get("icon_spear"));
        TexturedMaterial *iconWoodMat = dynamic_cast<TexturedMaterial *>(AssetLoader<Material>::get("icon_wood"));
        TexturedMaterial *iconFishMat = dynamic_cast<TexturedMaterial *>(AssetLoader<Material>::get("icon_fish"));
        Mesh *uiMesh = AssetLoader<Mesh>::get("plane");

        if (uiMat && uiMesh && hbMat && tbMat)
        {
            Entity *playerEntity = nullptr;
            Entity *raftEntity = nullptr;
            HealthComponent *playerHealth = nullptr;
            InventoryComponent *playerInventory = nullptr;
            HealthComponent *boatHealth = nullptr;

            for (auto entity : world->getEntities())
            {
                if (auto hp = entity->getComponent<HealthComponent>())
                {
                    if (entity->getComponent<InventoryComponent>())
                    {
                        playerHealth = hp;
                        playerEntity = entity;
                        playerInventory = entity->getComponent<InventoryComponent>();
                    }
                    else if (entity->name == "raft")
                    {
                        boatHealth = hp;
                        raftEntity = entity;
                    }
                }
            }

            auto drawQuad = [&](float x, float y, float w, float h, glm::vec4 color)
            {
                uiMat->tint = color;
                uiMat->shader->set("tint", color);
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + w / 2.0f, y + h / 2.0f, 0.0f));
                model = glm::scale(model, glm::vec3(w, h, 1.0f));
                uiMat->shader->set("transform", uiProj * model);
                uiMesh->draw();
            };

            auto drawHealthBarShader = [&](float x, float y, float r, float hp, glm::vec3 water_color)
            {
                hbMat->shader->set("time", (float)glfwGetTime());
                hbMat->shader->set("hp_percent", hp);
                hbMat->shader->set("water_color", water_color);

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + r, y + r, 0.0f));
                model = glm::scale(model, glm::vec3(r * 2.0f, r * 2.0f, 1.0f));
                hbMat->shader->set("transform", uiProj * model);
                uiMesh->draw();
            };

            auto drawToolbarSlot = [&](float x, float y, float size, bool isSelected, glm::vec3 baseCol)
            {
                tbMat->shader->set("time", (float)glfwGetTime());
                tbMat->shader->set("isSelected", isSelected ? 1 : 0);
                tbMat->shader->set("baseColor", baseCol);

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + size / 2.0f, y + size / 2.0f, 0.0f));
                model = glm::scale(model, glm::vec3(size, size, 1.0f));
                tbMat->shader->set("transform", uiProj * model);
                uiMesh->draw();
            };

            // Player Health Bar
            if (playerHealth)
            {
                float hpPercent = playerHealth->currentHealth / playerHealth->maxHealth;
                hbMat->setup();
                drawHealthBarShader(20, 20, 40, hpPercent, glm::vec3(0.8, 0.1, 0.1));
                hbMat->teardown();
            }

            // Boat Health Bar
            if (boatHealth)
            {
                float hpPercent = boatHealth->currentHealth / boatHealth->maxHealth;
                hbMat->setup();
                drawHealthBarShader(110, 20, 40, hpPercent, glm::vec3(0.5, 0.3, 0.1));
                hbMat->teardown();
            }

            // Inventory
            if (playerInventory)
            {
                float tbY = windowSize.y - 80.0f; // Bottom left
                glm::vec3 whiteGlass(0.5f, 0.5f, 0.5f);

                // Draw slots using the special shader
                tbMat->setup();
                drawToolbarSlot(30, tbY, 40, (playerInventory->activeSlot == 1), whiteGlass);
                drawToolbarSlot(120, tbY, 40, (playerInventory->activeSlot == 2), whiteGlass);
                drawToolbarSlot(210, tbY, 40, (playerInventory->activeSlot == 3), whiteGlass);
                // Draw resource slots using the special shader
                if (playerInventory->woodCount > 0)
                {
                    drawToolbarSlot(30, tbY - 55, 24, false, whiteGlass);
                }
                if (playerInventory->fishCount > 0)
                {
                    drawToolbarSlot(120, tbY - 55, 24, false, whiteGlass);
                }
                tbMat->teardown();

                auto drawIcon = [&](float x, float y, float size, TexturedMaterial *mat)
                {
                    if (!mat)
                        return;
                    mat->setup();
                    mat->shader->set("tint", glm::vec4(1.0f));
                    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + size / 2.0f, y + size / 2.0f, 0.0f));
                    model = glm::scale(model, glm::vec3(size, size, 1.0f));
                    mat->shader->set("transform", uiProj * model);
                    uiMesh->draw();
                    mat->teardown();
                };

                drawIcon(38, tbY + 8, 24, iconHammerMat);
                drawIcon(128, tbY + 8, 24, iconNetMat);
                drawIcon(218, tbY + 8, 24, iconSpearMat);

                if (playerInventory->woodCount > 0)
                {
                    drawIcon(34, tbY - 51, 16, iconWoodMat);
                }
                if (playerInventory->fishCount > 0)
                {
                    drawIcon(124, tbY - 51, 16, iconFishMat);
                }

                // --- MINIMAP RENDERING ---
                if (minimapMat && playerEntity)
                {
                    minimapMat->setup();
                    minimapMat->shader->set("time", (float)glfwGetTime());

                    // Get positions
                    glm::vec3 pPos = playerEntity->localTransform.position;

                    int entityCount = 0;
                    for (auto entity : world->getEntities())
                    {
                        if (entity == playerEntity)
                            continue;
                        if (entity->name == "ocean" || entity->name == "water" || entity->name == "sky" || entity->name == "moon" || entity->name.empty())
                            continue;

                        // Determine radar blip type
                        int typeIndex = -1;
                        if (entity->getComponent<EnemyComponent>()) {
                            typeIndex = 0; // enemy/shark = red
                        } else {
                            auto res = entity->getComponent<ResourceComponent>();
                            if (res) {
                                if (res->type == "fish") typeIndex = 1; // fish = blue
                                else if (res->type == "wood") typeIndex = 2; // wood = brown
                            }
                        }
                        if (typeIndex < 0)
                            continue; // Skip non-radar entities (raft, weapons, etc.)

                        glm::vec3 ePos = entity->localTransform.position;
                        glm::vec2 relativePos = glm::vec2(ePos.x - pPos.x, ePos.z - pPos.z) * 0.05f;

                        if (glm::length(relativePos) < 1.0f)
                        {
                            minimapMat->shader->set("entities_pos[" + std::to_string(entityCount) + "]", relativePos);
                            minimapMat->shader->set("entities_type[" + std::to_string(entityCount) + "]", (float)typeIndex);
                            entityCount++;
                            if (entityCount >= 32)
                                break;
                        }
                    }
                    minimapMat->shader->set("num_entities", entityCount);

                    // Draw in bottom right corner
                    float radarRadius = 50.0f;
                    float padding = 50.0f;
                    float minX = windowSize.x - (radarRadius * 2.0f) - padding;
                    float minY = windowSize.y - (radarRadius * 2.0f) - padding;

                    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(minX + radarRadius, minY + radarRadius, 0.0f));
                    model = glm::scale(model, glm::vec3(radarRadius * 2.0f, radarRadius * 2.0f, 1.0f));

                    minimapMat->shader->set("transform", uiProj * model);
                    uiMesh->draw();
                    minimapMat->teardown();
                }



                // Text is handled in Playstate::onImmediateGui() due to ImGui lifecycle
            }
// --- RADAR SKULL MARKER ---
if (skullTexture && playerEntity) {
    static double lastTime = glfwGetTime();
    double currentTime = glfwGetTime();
    float deltaTime = (float)(currentTime - lastTime);
    lastTime = currentTime;

    Entity* octopusEntity = nullptr;
    OctopusComponent* octopus = nullptr;
    for (auto entity : world->getEntities()) {
        if (auto occ = entity->getComponent<OctopusComponent>(); occ) {
            octopus = occ;
            octopusEntity = entity;
            break;
        }
    }
    if (octopus && octopus->permanentlyDead) {
        // Skip skull rendering when permanently dead
    } else

    if (!octopus || (int)octopus->state == (int)OctopusState::DYING || (int)octopus->state == (int)OctopusState::DEATH_ANIM || (int)octopus->state == (int)OctopusState::REVIVING || octopus->health <= 0.0f) {
        skullPulseTimer = 0.0f;
    } 
    else if ((int)octopus->state >= (int)OctopusState::COMBAT_IDLE) {
        skullPulseTimer += deltaTime;
        
        glm::vec3 pPos = playerEntity->localTransform.position;
        glm::vec3 ePos = octopusEntity->localTransform.position;
     // Get raw world offset
glm::vec2 worldOffset = glm::vec2(ePos.x - pPos.x, ePos.z - pPos.z) * 0.05f;

// Rotate by player's Y rotation so radar is player-relative
float playerRotY = playerEntity->localTransform.rotation.y;
float cosA = std::cos(-playerRotY);
float sinA = std::sin(-playerRotY);
glm::vec2 relativePos = glm::vec2(
    worldOffset.x * cosA - worldOffset.y * sinA,
    worldOffset.x * sinA + worldOffset.y * cosA
);

        {
            float radarRadius = 50.0f;
            float padding = 50.0f;
            float minX = windowSize.x - (radarRadius * 2.0f) - padding;
            float minY = windowSize.y - (radarRadius * 2.0f) - padding;
            float centerX = minX + radarRadius;
            float centerY = minY + radarRadius;

            // Clamp to radar edge if octopus is far away
            glm::vec2 clampedPos = relativePos;
            if (glm::length(relativePos) > 1.0f) {
                clampedPos = glm::normalize(relativePos) * 0.95f;
            }

            float dotX = centerX + clampedPos.x * radarRadius;
            float dotY = centerY - clampedPos.y * radarRadius;
            
            float regular_dot_radius = 4.0f;
            float baseSize = regular_dot_radius * 4.0f;
            float pulseScale = 1.0f + 0.2f * std::sin(skullPulseTimer * 3.0f);
            float drawSize = baseSize * pulseScale;
            
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(dotX - drawSize*0.5f, dotY - drawSize*0.5f, 0.0f));
            model = glm::scale(model, glm::vec3(drawSize, drawSize, 1.0f));
            
            glUseProgram(skullShaderProgram);
            
            GLint transformLoc = glGetUniformLocation(skullShaderProgram, "transform");
            glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(uiProj * model));
            
            GLint texLoc = glGetUniformLocation(skullShaderProgram, "uSkullTex");
            glUniform1i(texLoc, 0);
            
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, skullTexture);
            
            GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
            GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDisable(GL_DEPTH_TEST);
            
            glBindVertexArray(skullVAO);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            
            if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
            if (!blendWasEnabled) glDisable(GL_BLEND);
        }
    }
}
        }
    }

}
