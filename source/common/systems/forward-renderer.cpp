#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include "../components/light.hpp"
#include "../material/lit-material.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../components/health.hpp"
#include "../components/inventory.hpp"
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <string>

namespace our {

    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json& config){
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Then we check if there is a sky texture in the configuration
        if(config.contains("sky")){
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));
            
            // We can draw the sky using the same shader used to draw textured objects (or our procedural one)
            ShaderProgram* skyShader = new ShaderProgram();
            if (config.value<std::string>("sky", "") == "procedural") {
                skyShader->attach("assets/shaders/procedural_sky.vert", GL_VERTEX_SHADER);
                skyShader->attach("assets/shaders/procedural_sky.frag", GL_FRAGMENT_SHADER);
            } else {
                skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
                skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
            }
            skyShader->link();
            
            //TODO: (Req 10) Pick the correct pipeline state to draw the sky
            // Hints: the sky will be draw after the opaque objects so we would need depth testing but which depth funtion should we pick?
            // We will draw the sphere from the inside, so what options should we pick for the face culling.
            PipelineState skyPipelineState{};
            skyPipelineState.depthTesting.enabled = true;
            skyPipelineState.depthTesting.function = GL_LEQUAL;
            skyPipelineState.faceCulling.enabled = true;
            skyPipelineState.faceCulling.culledFace = GL_FRONT;
            skyPipelineState.faceCulling.frontFace = GL_CCW;
            
            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
            std::string skyTextureFile = config.value<std::string>("sky", "");
            
            if (skyTextureFile == "procedural" || skyTextureFile.empty()) {
                this->skyMaterial = new Material();
                this->skyMaterial->shader = skyShader;
                this->skyMaterial->pipelineState = skyPipelineState;
                this->skyMaterial->transparent = false;
            } else {
                Texture2D* skyTexture = texture_utils::loadImage(skyTextureFile, false);

                // Setup a sampler for the sky 
                Sampler* skySampler = new Sampler();
                skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
                skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                // Combine all the aforementioned objects (except the mesh) into a material 
                TexturedMaterial* texMat = new TexturedMaterial();
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
        if(config.contains("postprocess")){
            //TODO: (Req 11) Create a framebuffer
            glGenFramebuffers(1, &postprocessFrameBuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);

            //TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
            // Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
            // The depth format can be (Depth component with 24 bits).
            colorTarget = texture_utils::empty(GL_RGBA8, windowSize);
            depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT24, windowSize);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);
            
            //TODO: (Req 11) Unbind the framebuffer just to be safe
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Create a vertex array to use for drawing the texture
            glGenVertexArrays(1, &postProcessVertexArray);

            // Create a sampler to use for sampling the scene texture in the post processing shader
            Sampler* postprocessSampler = new Sampler();
            postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Create the post processing shader
            ShaderProgram* postprocessShader = new ShaderProgram();
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
    }

    void ForwardRenderer::destroy(){
        // Delete all objects related to the sky
        if(skyMaterial){
            delete skySphere;
            delete skyMaterial->shader;
            if (auto texMat = dynamic_cast<TexturedMaterial*>(skyMaterial); texMat) {
                delete texMat->texture;
                delete texMat->sampler;
            }
            delete skyMaterial;
        }
        // Delete all objects related to post processing
        if(postprocessMaterial){
            glDeleteFramebuffers(1, &postprocessFrameBuffer);
            glDeleteVertexArrays(1, &postProcessVertexArray);
            delete colorTarget;
            delete depthTarget;
            delete postprocessMaterial->sampler;
            delete postprocessMaterial->shader;
            delete postprocessMaterial;
        }
    }

    void ForwardRenderer::render(World* world){
        // First of all, we search for a camera and for all the mesh renderers
        CameraComponent* camera = nullptr;
        opaqueCommands.clear();
        transparentCommands.clear();
        for(auto entity : world->getEntities()){
            // If we hadn't found a camera yet, we look for a camera in this entity
            if(!camera) camera = entity->getComponent<CameraComponent>();
            // If this entity has a mesh renderer component
            if(auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer){
                // We construct a command from it
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;
                // if it is transparent, we add it to the transparent commands list
                if(command.material->transparent){
                    transparentCommands.push_back(command);
                } else {
                // Otherwise, we add it to the opaque command list
                    opaqueCommands.push_back(command);
                }
            }
        }
        
        // Gather all light components
        std::vector<std::pair<LightComponent*, glm::mat4>> lights;
        for(auto entity : world->getEntities()){
            if(auto light = entity->getComponent<LightComponent>(); light){
                lights.push_back({light, entity->getLocalToWorldMatrix()});
            }
        }

        glm::mat4 cameraLocalToWorld = camera->getOwner()->getLocalToWorldMatrix();
        glm::vec3 cameraPosition = glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, 0, 1));

        //TODO: (Req 9) Modify the following line such that "cameraForward" contains a vector pointing the camera forward direction
        // HINT: See how you wrote the CameraComponent::getViewMatrix, it should help you solve this one
        glm::vec3 cameraForward = glm::normalize(glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, -1, 0)));
        std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraForward, cameraPosition](const RenderCommand& first, const RenderCommand& second){
            //TODO: (Req 9) Finish this function
            // HINT: the following return should return true "first" should be drawn before "second". 
            float firstDepth = glm::dot(first.center - cameraPosition, cameraForward);
            float secondDepth = glm::dot(second.center - cameraPosition, cameraForward);
            return firstDepth > secondDepth;
        });

        //TODO: (Req 9) Get the camera ViewProjection matrix and store it in VP
        glm::ivec2 viewportStart(0, 0);
        glm::ivec2 viewportSize = windowSize;
        glm::mat4 VP = camera->getProjectionMatrix(viewportSize) * camera->getViewMatrix();
        
        //TODO: (Req 9) Set the OpenGL viewport using viewportStart and viewportSize
        glViewport(viewportStart.x, viewportStart.y, viewportSize.x, viewportSize.y);
        
        //TODO: (Req 9) Set the clear color to black and the clear depth to 1
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearDepth(1.0f);
        
        //TODO: (Req 9) Set the color mask to true and the depth mask to true (to ensure the glClear will affect the framebuffer)
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_TRUE);
        

        // If there is a postprocess material, bind the framebuffer
        if(postprocessMaterial){
            //TODO: (Req 11) bind the framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);
            
        }

        //TODO: (Req 9) Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        //TODO: (Req 9) Draw all the opaque commands
        // Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for(const auto& command : opaqueCommands){
            command.material->setup();
            
            // Set lighting uniforms if applicable
            if (auto lit_material = dynamic_cast<LitMaterial*>(command.material); lit_material != nullptr) {
                // Determine camera position from the 'camera' component's view matrix directly as it might be easier
                glm::mat4 V = camera->getViewMatrix();
                glm::vec3 eye = glm::vec3(glm::inverse(V)[3]);

                float u_time = (float)glfwGetTime();
                
                // Keep the exact same day/night speed and vector as the shader
                float dayNightSpeed = 0.05f;
                float skyTime = u_time * dayNightSpeed;
                
                // Sun rotates
                glm::vec3 sun_dir = glm::normalize(glm::vec3(0.0f, std::sin(skyTime), -std::cos(skyTime)));
                
                // Color fades when going below the horizon
                float height = std::max(sun_dir.y, 0.0f);
                glm::vec3 sun_col_day = glm::vec3(1.0f, 0.9f, 0.8f);
                glm::vec3 sun_col_sunset = glm::vec3(1.0f, 0.4f, 0.2f);
                glm::vec3 sun_color = glm::mix(sun_col_sunset, sun_col_day, height);
                // Ramp intensity down gently at the horizon
                float intensity = 3.0f * glm::smoothstep(-0.1f, 0.1f, sun_dir.y);

                lit_material->shader->set("camera_position", eye);
                lit_material->shader->set("light_count", (int)lights.size() + 1);
                
                // Add the sun as the first light
                lit_material->shader->set("lights[0].type", 0);
                lit_material->shader->set("lights[0].color", sun_color);
                lit_material->shader->set("lights[0].intensity", intensity); 
                lit_material->shader->set("lights[0].position", -sun_dir);

                for(size_t i = 0; i < lights.size(); ++i) {
                    auto* light = lights[i].first;
                    glm::mat4 lightTrans = lights[i].second;
                    std::string lightStr = "lights[" + std::to_string(i + 1) + "]";
                    
                    lit_material->shader->set(lightStr + ".type", light->type == LightType::POINT ? 1 : 0);
                    lit_material->shader->set(lightStr + ".color", light->color);
                    lit_material->shader->set(lightStr + ".intensity", light->intensity);
                    
                    if (light->type == LightType::POINT) {
                        lit_material->shader->set(lightStr + ".position", glm::vec3(lightTrans[3]));
                    } else { 
                        // For directional lights, position acts as the direction vector
                        glm::vec3 direction = glm::normalize(glm::vec3(lightTrans * glm::vec4(0, 0, -1, 0)));
                        lit_material->shader->set(lightStr + ".position", direction);
                    }
                }
                
                lit_material->shader->set("M", command.localToWorld);
                lit_material->shader->set("M_IT", glm::transpose(glm::inverse(command.localToWorld)));
                lit_material->shader->set("VP", VP);
            }
            
            command.material->shader->set("transform", VP * command.localToWorld);
            command.mesh->draw();
            command.material->teardown();
        }
        
        // If there is a sky material, draw the sky
        if(this->skyMaterial){
            //TODO: (Req 10) setup the sky material
            skyMaterial->setup();
            
            //TODO: (Req 10) Get the camera position
            glm::vec3 cameraPosition = glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, 0, 1));
            
            //TODO: (Req 10) Create a model matrix for the sy such that it always follows the camera (sky sphere center = camera position)
            glm::mat4 skyModel = glm::translate(glm::mat4(1.0f), cameraPosition);
            
            //TODO: (Req 10) We want the sky to be drawn behind everything (in NDC space, z=1)
            // We can acheive the is by multiplying by an extra matrix after the projection but what values should we put in it?
            glm::mat4 alwaysBehindTransform = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f
            );
            //TODO: (Req 10) set the "transform" uniform
            skyMaterial->shader->set("transform", alwaysBehindTransform * VP * skyModel);
            skyMaterial->shader->set("camera_position", cameraPosition);
            skyMaterial->shader->set("u_time", (float)glfwGetTime());
            
            //TODO: (Req 10) draw the sky sphere
            skySphere->draw();
            skyMaterial->teardown();
        }
        //TODO: (Req 9) Draw all the transparent commands
        // Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for(const auto& command : transparentCommands){
            command.material->setup();
            
            if (auto lit_material = dynamic_cast<LitMaterial*>(command.material); lit_material != nullptr) {
                glm::mat4 V = camera->getViewMatrix();
                glm::vec3 eye = glm::vec3(glm::inverse(V)[3]);

                float u_time = (float)glfwGetTime();
                
                // Keep the exact same day/night speed and vector as the shader
                float dayNightSpeed = 0.05f;
                float skyTime = u_time * dayNightSpeed;
                
                // Sun rotates
                glm::vec3 sun_dir = glm::normalize(glm::vec3(0.0f, std::sin(skyTime), -std::cos(skyTime)));
                
                // Color fades when going below the horizon
                float height = std::max(sun_dir.y, 0.0f);
                glm::vec3 sun_col_day = glm::vec3(1.0f, 0.9f, 0.8f);
                glm::vec3 sun_col_sunset = glm::vec3(1.0f, 0.4f, 0.2f);
                glm::vec3 sun_color = glm::mix(sun_col_sunset, sun_col_day, height);
                // Ramp intensity down gently at the horizon
                float intensity = 3.0f * glm::smoothstep(-0.1f, 0.1f, sun_dir.y);

                lit_material->shader->set("camera_position", eye);
                lit_material->shader->set("light_count", (int)lights.size() + 1);
                
                // Add the sun as the first light
                lit_material->shader->set("lights[0].type", 0);
                lit_material->shader->set("lights[0].color", sun_color);
                lit_material->shader->set("lights[0].intensity", intensity); 
                lit_material->shader->set("lights[0].position", -sun_dir);

                for(size_t i = 0; i < lights.size(); ++i) {
                    auto* light = lights[i].first;
                    glm::mat4 lightTrans = lights[i].second;
                    std::string lightStr = "lights[" + std::to_string(i + 1) + "]";
                    
                    lit_material->shader->set(lightStr + ".type", light->type == LightType::POINT ? 1 : 0);
                    lit_material->shader->set(lightStr + ".color", light->color);
                    lit_material->shader->set(lightStr + ".intensity", light->intensity);
                    
                    if (light->type == LightType::POINT) {
                        lit_material->shader->set(lightStr + ".position", glm::vec3(lightTrans[3]));
                    } else {
                        glm::vec3 direction = glm::normalize(glm::vec3(lightTrans * glm::vec4(0, 0, -1, 0)));
                        lit_material->shader->set(lightStr + ".position", direction);
                    }
                }
                
                lit_material->shader->set("M", command.localToWorld);
                lit_material->shader->set("M_IT", glm::transpose(glm::inverse(command.localToWorld)));
                lit_material->shader->set("VP", VP);
            }
            
            command.material->shader->set("transform", VP * command.localToWorld);
            command.mesh->draw();
            command.material->teardown();
        }
        

        // If there is a postprocess material, apply postprocessing
        if(postprocessMaterial){
            //TODO: (Req 11) Return to the default framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            
            //TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
            postprocessMaterial->setup();
            glBindVertexArray(postProcessVertexArray);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindVertexArray(0);
            postprocessMaterial->teardown();
        }

        // Phase 2 UI Pass
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glm::mat4 uiProj = glm::ortho(0.0f, (float)windowSize.x, (float)windowSize.y, 0.0f, -1.0f, 1.0f);
        
        TintedMaterial* uiMat = dynamic_cast<TintedMaterial*>(AssetLoader<Material>::get("tinted"));
        TintedMaterial* hbMat = dynamic_cast<TintedMaterial*>(AssetLoader<Material>::get("health-bar"));
        TintedMaterial* minimapMat = dynamic_cast<TintedMaterial*>(AssetLoader<Material>::get("minimap"));
        TintedMaterial* tbMat = dynamic_cast<TintedMaterial*>(AssetLoader<Material>::get("toolbar"));
        TexturedMaterial* iconHammerMat = dynamic_cast<TexturedMaterial*>(AssetLoader<Material>::get("icon_hammer"));
        TexturedMaterial* iconNetMat = dynamic_cast<TexturedMaterial*>(AssetLoader<Material>::get("icon_net"));
        TexturedMaterial* iconSpearMat = dynamic_cast<TexturedMaterial*>(AssetLoader<Material>::get("icon_spear"));
        TexturedMaterial* iconWoodMat = dynamic_cast<TexturedMaterial*>(AssetLoader<Material>::get("icon_wood"));
        TexturedMaterial* iconFishMat = dynamic_cast<TexturedMaterial*>(AssetLoader<Material>::get("icon_fish"));
        Mesh* uiMesh = AssetLoader<Mesh>::get("plane");
        
        if (uiMat && uiMesh && hbMat && tbMat) {
            Entity* playerEntity = nullptr;
            Entity* raftEntity = nullptr;
            HealthComponent* playerHealth = nullptr;
            InventoryComponent* playerInventory = nullptr;
            HealthComponent* boatHealth = nullptr;
            
            for (auto entity : world->getEntities()) {
                if (auto hp = entity->getComponent<HealthComponent>()) {
                    if (entity->getComponent<InventoryComponent>()) {
                        playerHealth = hp;
                        playerEntity = entity;
                        playerInventory = entity->getComponent<InventoryComponent>();
                    } else if (entity->name == "raft") {
                        boatHealth = hp;
                        raftEntity = entity;
                    }
                }
            }

            auto drawQuad = [&](float x, float y, float w, float h, glm::vec4 color) {
                uiMat->tint = color;
                uiMat->shader->set("tint", color);
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + w/2.0f, y + h/2.0f, 0.0f));
                model = glm::scale(model, glm::vec3(w, h, 1.0f));
                uiMat->shader->set("transform", uiProj * model);
                uiMesh->draw();
            };

            auto drawHealthBarShader = [&](float x, float y, float r, float hp, glm::vec3 water_color) {
                hbMat->shader->set("time", (float)glfwGetTime());
                hbMat->shader->set("hp_percent", hp);
                hbMat->shader->set("water_color", water_color);
                
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + r, y + r, 0.0f));
                model = glm::scale(model, glm::vec3(r * 2.0f, r * 2.0f, 1.0f));
                hbMat->shader->set("transform", uiProj * model);
                uiMesh->draw();
            };

            auto drawToolbarSlot = [&](float x, float y, float size, bool isSelected, glm::vec3 baseCol) {
                tbMat->shader->set("time", (float)glfwGetTime());
                tbMat->shader->set("isSelected", isSelected ? 1 : 0);
                tbMat->shader->set("baseColor", baseCol);
                
                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + size/2.0f, y + size/2.0f, 0.0f));
                model = glm::scale(model, glm::vec3(size, size, 1.0f));
                tbMat->shader->set("transform", uiProj * model);
                uiMesh->draw();
            };

            // Player Health Bar
            if (playerHealth) {
                float hpPercent = playerHealth->currentHealth / playerHealth->maxHealth;
                hbMat->setup();
                drawHealthBarShader(20, 20, 40, hpPercent, glm::vec3(0.8, 0.1, 0.1));
                hbMat->teardown();
            }
            
            // Boat Health Bar
            if (boatHealth) {
                float hpPercent = boatHealth->currentHealth / boatHealth->maxHealth;
                hbMat->setup();
                drawHealthBarShader(110, 20, 40, hpPercent, glm::vec3(0.5, 0.3, 0.1));
                hbMat->teardown();
            }

            // Inventory
            if (playerInventory) {
                float tbY = windowSize.y - 80.0f; // Bottom left
                glm::vec3 whiteGlass(0.5f, 0.5f, 0.5f);
                
                // Draw slots using the special shader
                tbMat->setup();
                drawToolbarSlot(30, tbY, 40, (playerInventory->activeSlot == 1), whiteGlass);
                drawToolbarSlot(120, tbY, 40, (playerInventory->activeSlot == 2), whiteGlass);
                drawToolbarSlot(210, tbY, 40, (playerInventory->activeSlot == 3), whiteGlass);
                // Draw resource slots using the special shader
                if (playerInventory->woodCount > 0) {
                    drawToolbarSlot(30, tbY - 55, 24, false, whiteGlass);
                }
                if (playerInventory->fishCount > 0) {
                    drawToolbarSlot(120, tbY - 55, 24, false, whiteGlass);
                }
                tbMat->teardown();

                auto drawIcon = [&](float x, float y, float size, TexturedMaterial* mat) {
                    if(!mat) return;
                    mat->setup();
                    mat->shader->set("tint", glm::vec4(1.0f));
                    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(x + size/2.0f, y + size/2.0f, 0.0f));
                    model = glm::scale(model, glm::vec3(size, size, 1.0f));
                    mat->shader->set("transform", uiProj * model);
                    uiMesh->draw();
                    mat->teardown();
                };

                drawIcon(38, tbY + 8, 24, iconHammerMat);
                drawIcon(128, tbY + 8, 24, iconNetMat);
                drawIcon(218, tbY + 8, 24, iconSpearMat);

                if (playerInventory->woodCount > 0) {
                    drawIcon(34, tbY - 51, 16, iconWoodMat);
                }
                if (playerInventory->fishCount > 0) {
                    drawIcon(124, tbY - 51, 16, iconFishMat);
                }

                // --- MINIMAP RENDERING ---
                if (minimapMat && playerEntity && raftEntity) {
                    minimapMat->setup();
                    minimapMat->shader->set("time", (float)glfwGetTime());
                    
                    // Get positions
                    glm::vec3 pPos = playerEntity->localTransform.position;
                    glm::vec3 rPos = raftEntity->localTransform.position;
                    
                    // Calculate relative vector in XZ plane (scaled down so things fit on radar)
                    glm::vec2 relativePos = glm::vec2(rPos.x - pPos.x, rPos.z - pPos.z) * 0.05f; 
                    minimapMat->shader->set("raft_relative_pos", relativePos);
                    
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
        }
    }

}