#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../components/health.hpp"
#include "../components/inventory.hpp"
#include <GLFW/glfw3.h>

namespace our {

    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json& config){
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Then we check if there is a sky texture in the configuration
        if(config.contains("sky")){
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));
            
            // We can draw the sky using the same shader used to draw textured objects
            ShaderProgram* skyShader = new ShaderProgram();
            skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
            skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
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
            Texture2D* skyTexture = texture_utils::loadImage(skyTextureFile, false);

            // Setup a sampler for the sky 
            Sampler* skySampler = new Sampler();
            skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
            skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Combine all the aforementioned objects (except the mesh) into a material 
            this->skyMaterial = new TexturedMaterial();
            this->skyMaterial->shader = skyShader;
            this->skyMaterial->texture = skyTexture;
            this->skyMaterial->sampler = skySampler;
            this->skyMaterial->pipelineState = skyPipelineState;
            this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            this->skyMaterial->alphaThreshold = 1.0f;
            this->skyMaterial->transparent = false;
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
            delete skyMaterial->texture;
            delete skyMaterial->sampler;
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

        // If there is no camera, we return (we cannot render without a camera)
        if(camera == nullptr) return;

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
            
            //TODO: (Req 10) draw the sky sphere
            skySphere->draw();
            skyMaterial->teardown();
        }
        //TODO: (Req 9) Draw all the transparent commands
        // Don't forget to set the "transform" uniform to be equal the model-view-projection matrix for each render command
        for(const auto& command : transparentCommands){
            command.material->setup();
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
        Mesh* uiMesh = AssetLoader<Mesh>::get("plane");
        
        if (uiMat && uiMesh && hbMat) {
            HealthComponent* playerHealth = nullptr;
            InventoryComponent* playerInventory = nullptr;
            HealthComponent* boatHealth = nullptr;
            
            for (auto entity : world->getEntities()) {
                if (auto hp = entity->getComponent<HealthComponent>()) {
                    if (entity->getComponent<InventoryComponent>()) {
                        playerHealth = hp;
                        playerInventory = entity->getComponent<InventoryComponent>();
                    } else if (entity->name == "boat") {
                        boatHealth = hp;
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
                drawHealthBarShader(20, 110, 40, hpPercent, glm::vec3(0.1, 0.4, 0.8));
                hbMat->teardown();
            }

            uiMat->setup();
            // Inventory
            if (playerInventory) {
                // UI colors for slots
                glm::vec4 c1 = (playerInventory->activeSlot == 1) ? glm::vec4(1, 0.8, 0.2, 1) : glm::vec4(0.4, 0.4, 0.4, 1);
                glm::vec4 c2 = (playerInventory->activeSlot == 2) ? glm::vec4(1, 0.8, 0.2, 1) : glm::vec4(0.4, 0.4, 0.4, 1);
                glm::vec4 c3 = (playerInventory->activeSlot == 3) ? glm::vec4(1, 0.8, 0.2, 1) : glm::vec4(0.4, 0.4, 0.4, 1);

                // Draw slots
                drawQuad(20, 200, 40, 40, c1);
                drawQuad(70, 200, 40, 40, c2);
                drawQuad(120, 200, 40, 40, c3);

                // Draw indicators for resources (small squares)
                if (playerInventory->woodCount > 0) {
                    drawQuad(200, 200, 10, 10, glm::vec4(0.5, 0.3, 0.1, 1)); // Brown for Wood
                }
                if (playerInventory->fishCount > 0) {
                    drawQuad(200, 220, 10, 10, glm::vec4(0.2, 0.6, 1.0, 1)); // Blue for Fish
                }
            }

            uiMat->teardown();
        }
    }

}