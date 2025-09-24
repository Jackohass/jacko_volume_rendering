#include "../../defines.h"
#include "rcMineRenderer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>

#include <vis_utils/camera.h>

#include <volvis_utils/utils.h>
#include <math_utils/utils.h>

#include "imgui.h"
#include "imgui_impl_glut.h"
#include "imgui_impl_opengl2.h"

RayCastingMine::RayCastingMine()
    : m_glsl_transfer_function(nullptr)
    , cp_shader_rendering(nullptr)
    , m_u_step_size(0.5f)
    , m_apply_gradient_shading(false)
{
#ifdef MULTISAMPLE_AVAILABLE
    vr_pixel_multiscaling_support = true;
#endif
}

RayCastingMine::~RayCastingMine()
{
    Clean();

    glDisable(GL_DEPTH_TEST);
}

void RayCastingMine::Clean()
{
    if (m_glsl_transfer_function) delete m_glsl_transfer_function;
    m_glsl_transfer_function = nullptr;

    DestroyRenderingPass();

    BaseVolumeRenderer::Clean();
}

void RayCastingMine::ReloadShaders()
{
    cp_shader_rendering->Reload();
    m_rdr_frame_to_screen.ClearShaders();
}

void APIENTRY glDebugOutput(GLenum source,
    GLenum type,
    unsigned int id,
    GLenum severity,
    GLsizei length,
    const char* message,
    const void* userParam)
{
    // ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

    std::cout << "---------------" << std::endl;
    std::cout << "Debug message (" << id << "): " << message << std::endl;

    switch (source)
    {
    case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
    case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
    case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
    } std::cout << std::endl;

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
    case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
    case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
    case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
    case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
    case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
    case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
    } std::cout << std::endl;

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
    case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
    case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
    case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
    } std::cout << std::endl;
    std::cout << std::endl;
}

bool RayCastingMine::Init(int swidth, int sheight)
{
    if (IsBuilt()) Clean();

    if (m_ext_data_manager->GetCurrentVolumeTexture() == nullptr) return false;
    m_glsl_transfer_function = m_ext_data_manager->GetCurrentTransferFunction()->GenerateTexture_1D_RGBt();

    // Create Rendering Buffers and Shaders
    CreateRenderingPass();
    gl::ExitOnGLError("RayCastingMine: Error on Preparing Models and Shaders");

    // estimate initial integration step
    glm::dvec3 sv = m_ext_data_manager->GetCurrentStructuredVolume()->GetScale();
    m_u_step_size = float((0.5f / glm::sqrt(3.0f)) * glm::sqrt(sv.x * sv.x + sv.y * sv.y + sv.z * sv.z));

    Reshape(swidth, sheight);

    SetBuilt(true);
    SetOutdated();

    glEnable(GL_DEPTH_TEST);

    

    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugOutput, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

    int flags; glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) printf("DEBUG MODE ACTIVATED\n");
    
    if(glIsEnabled(GL_DEPTH_TEST)) printf("Depth test is enabled!\n");
    else printf("Depth test is NOT enabled!\n");

    return true;
}

bool RayCastingMine::UpdateMultiDraw(vis::Camera* camera)
{
    //first_pass_shader
    //render_shader_mutli
    //depth_shader_mutli

    // ############## First shader
    first_pass_shader->Bind();

    first_pass_shader->SetUniform("CameraEye", camera->GetEye());
    first_pass_shader->BindUniform("CameraEye");

    first_pass_shader->SetUniform("u_CameraLookAt", camera->LookAt());
    first_pass_shader->BindUniform("u_CameraLookAt");

    first_pass_shader->SetUniform("ProjectionMatrix", camera->Projection());
    first_pass_shader->BindUniform("ProjectionMatrix");

    first_pass_shader->SetUniform("u_TanCameraFovY", (float)tan(DEGREE_TO_RADIANS(camera->GetFovY()) / 2.0));
    first_pass_shader->BindUniform("u_TanCameraFovY");

    first_pass_shader->SetUniform("u_CameraAspectRatio", camera->GetAspectRatio());
    first_pass_shader->BindUniform("u_CameraAspectRatio");

    first_pass_shader->BindUniforms();
    gl::Shader::Unbind();
    gl::ExitOnGLError("RayCastingMine Render shader: After Update.");

    // ############## Depth full shader
    depth_shader_full->Bind();

    depth_shader_full->SetUniform("CameraEye", camera->GetEye());
    depth_shader_full->BindUniform("CameraEye");

    depth_shader_full->SetUniform("u_CameraLookAt", camera->LookAt());
    depth_shader_full->BindUniform("u_CameraLookAt");

    depth_shader_full->SetUniform("ProjectionMatrix", camera->Projection());
    depth_shader_full->BindUniform("ProjectionMatrix");

    depth_shader_full->SetUniform("u_TanCameraFovY", (float)tan(DEGREE_TO_RADIANS(camera->GetFovY()) / 2.0));
    depth_shader_full->BindUniform("u_TanCameraFovY");

    depth_shader_full->SetUniform("u_CameraAspectRatio", camera->GetAspectRatio());
    depth_shader_full->BindUniform("u_CameraAspectRatio");

    depth_shader_full->BindUniforms();
    gl::Shader::Unbind();
    gl::ExitOnGLError("RayCastingMine Render shader: After Update.");

    // ############## Render shader
    render_shader_mutli->Bind();

    render_shader_mutli->SetUniform("CameraEye", camera->GetEye());
    render_shader_mutli->BindUniform("CameraEye");
                 
    render_shader_mutli->SetUniform("u_CameraLookAt", camera->LookAt());
    render_shader_mutli->BindUniform("u_CameraLookAt");
                 
    render_shader_mutli->SetUniform("ProjectionMatrix", camera->Projection());
    render_shader_mutli->BindUniform("ProjectionMatrix");
                 
    render_shader_mutli->SetUniform("u_TanCameraFovY", (float)tan(DEGREE_TO_RADIANS(camera->GetFovY()) / 2.0));
    render_shader_mutli->BindUniform("u_TanCameraFovY");
                 
    render_shader_mutli->SetUniform("u_CameraAspectRatio", camera->GetAspectRatio());
    render_shader_mutli->BindUniform("u_CameraAspectRatio");
                 
    render_shader_mutli->SetUniform("StepSize", m_u_step_size);
    render_shader_mutli->BindUniform("StepSize");
                 
    render_shader_mutli->SetUniform("ApplyOcclusion", 1);
    render_shader_mutli->BindUniform("ApplyOcclusion");
                 
    render_shader_mutli->SetUniform("ApplyShadow", 1);
    render_shader_mutli->BindUniform("ApplyShadow");
                 
    render_shader_mutli->SetUniform("ApplyGradientPhongShading", (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture()) ? 1 : 0);
    render_shader_mutli->BindUniform("ApplyGradientPhongShading");
                 
    render_shader_mutli->SetUniform("BlinnPhongKa", m_ext_rendering_parameters->GetBlinnPhongKambient());
    render_shader_mutli->BindUniform("BlinnPhongKa");
    render_shader_mutli->SetUniform("BlinnPhongKd", m_ext_rendering_parameters->GetBlinnPhongKdiffuse());
    render_shader_mutli->BindUniform("BlinnPhongKd");
    render_shader_mutli->SetUniform("BlinnPhongKs", m_ext_rendering_parameters->GetBlinnPhongKspecular());
    render_shader_mutli->BindUniform("BlinnPhongKs");
    render_shader_mutli->SetUniform("BlinnPhongShininess", m_ext_rendering_parameters->GetBlinnPhongNshininess());
    render_shader_mutli->BindUniform("BlinnPhongShininess");
                 
    render_shader_mutli->SetUniform("BlinnPhongIspecular", m_ext_rendering_parameters->GetLightSourceSpecular());
    render_shader_mutli->BindUniform("BlinnPhongIspecular");
                 
    render_shader_mutli->SetUniform("WorldEyePos", camera->GetEye());
    render_shader_mutli->BindUniform("WorldEyePos");
                 
    render_shader_mutli->SetUniform("LightSourcePosition", m_ext_rendering_parameters->GetBlinnPhongLightingPosition());
    render_shader_mutli->BindUniform("LightSourcePosition");
                 
    render_shader_mutli->BindUniforms();

    gl::Shader::Unbind();
    gl::ExitOnGLError("RayCastingMine Render shader: After Update.");

    // ############## Depth shader
    /*None*/

    return true;
}

bool RayCastingMine::UpdateFirstpass(vis::Camera* camera)
{
    render_shader->Bind();

    render_shader->SetUniform("CameraEye", camera->GetEye());
    render_shader->BindUniform("CameraEye");

    render_shader->SetUniform("u_CameraLookAt", camera->LookAt());
    render_shader->BindUniform("u_CameraLookAt");

    render_shader->SetUniform("ProjectionMatrix", camera->Projection());
    render_shader->BindUniform("ProjectionMatrix");

    render_shader->SetUniform("u_TanCameraFovY", (float)tan(DEGREE_TO_RADIANS(camera->GetFovY()) / 2.0));
    render_shader->BindUniform("u_TanCameraFovY");

    render_shader->SetUniform("u_CameraAspectRatio", camera->GetAspectRatio());
    render_shader->BindUniform("u_CameraAspectRatio");

    render_shader->SetUniform("StepSize", m_u_step_size);
    render_shader->BindUniform("StepSize");

    render_shader->SetUniform("ApplyOcclusion", 1);
    render_shader->BindUniform("ApplyOcclusion");

    render_shader->SetUniform("ApplyShadow", 1);
    render_shader->BindUniform("ApplyShadow");

    render_shader->SetUniform("ApplyGradientPhongShading", (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture()) ? 1 : 0);
    render_shader->BindUniform("ApplyGradientPhongShading");

    render_shader->SetUniform("BlinnPhongKa", m_ext_rendering_parameters->GetBlinnPhongKambient());
    render_shader->BindUniform("BlinnPhongKa");
    render_shader->SetUniform("BlinnPhongKd", m_ext_rendering_parameters->GetBlinnPhongKdiffuse());
    render_shader->BindUniform("BlinnPhongKd");
    render_shader->SetUniform("BlinnPhongKs", m_ext_rendering_parameters->GetBlinnPhongKspecular());
    render_shader->BindUniform("BlinnPhongKs");
    render_shader->SetUniform("BlinnPhongShininess", m_ext_rendering_parameters->GetBlinnPhongNshininess());
    render_shader->BindUniform("BlinnPhongShininess");

    render_shader->SetUniform("BlinnPhongIspecular", m_ext_rendering_parameters->GetLightSourceSpecular());
    render_shader->BindUniform("BlinnPhongIspecular");

    render_shader->SetUniform("WorldEyePos", camera->GetEye());
    render_shader->BindUniform("WorldEyePos");

    render_shader->SetUniform("LightSourcePosition", m_ext_rendering_parameters->GetBlinnPhongLightingPosition());
    render_shader->BindUniform("LightSourcePosition");

    render_shader->BindUniforms();

    gl::Shader::Unbind();
    gl::ExitOnGLError("RayCastingMine Render shader: After Update.");
    return true;
}

bool RayCastingMine::Update(vis::Camera* camera)
{
    cp_shader_rendering->Bind();

    // MULTISAMPLE
    if (IsPixelMultiScalingSupported() && GetCurrentMultiScalingMode() > 0)
    {
        cp_shader_rendering->RecomputeNumberOfGroups(m_rdr_frame_to_screen.GetWidth(),
            m_rdr_frame_to_screen.GetHeight(), 0);
    }
    else
    {
        cp_shader_rendering->RecomputeNumberOfGroups(m_ext_rendering_parameters->GetScreenWidth(),
            m_ext_rendering_parameters->GetScreenHeight(), 0);
    }

    cp_shader_rendering->SetUniform("CameraEye", camera->GetEye());
    cp_shader_rendering->BindUniform("CameraEye");

    cp_shader_rendering->SetUniform("u_CameraLookAt", camera->LookAt());
    cp_shader_rendering->BindUniform("u_CameraLookAt");

    cp_shader_rendering->SetUniform("ProjectionMatrix", camera->Projection());
    cp_shader_rendering->BindUniform("ProjectionMatrix");

    cp_shader_rendering->SetUniform("u_TanCameraFovY", (float)tan(DEGREE_TO_RADIANS(camera->GetFovY()) / 2.0));
    cp_shader_rendering->BindUniform("u_TanCameraFovY");

    cp_shader_rendering->SetUniform("u_CameraAspectRatio", camera->GetAspectRatio());
    cp_shader_rendering->BindUniform("u_CameraAspectRatio");

    cp_shader_rendering->SetUniform("StepSize", m_u_step_size);
    cp_shader_rendering->BindUniform("StepSize");

    cp_shader_rendering->SetUniform("ApplyOcclusion", 1);
    cp_shader_rendering->BindUniform("ApplyOcclusion");

    cp_shader_rendering->SetUniform("ApplyShadow", 1);
    cp_shader_rendering->BindUniform("ApplyShadow");

    cp_shader_rendering->SetUniform("ApplyGradientPhongShading", (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture()) ? 1 : 0);
    cp_shader_rendering->BindUniform("ApplyGradientPhongShading");

    cp_shader_rendering->SetUniform("BlinnPhongKa", m_ext_rendering_parameters->GetBlinnPhongKambient());
    cp_shader_rendering->BindUniform("BlinnPhongKa");
    cp_shader_rendering->SetUniform("BlinnPhongKd", m_ext_rendering_parameters->GetBlinnPhongKdiffuse());
    cp_shader_rendering->BindUniform("BlinnPhongKd");
    cp_shader_rendering->SetUniform("BlinnPhongKs", m_ext_rendering_parameters->GetBlinnPhongKspecular());
    cp_shader_rendering->BindUniform("BlinnPhongKs");
    cp_shader_rendering->SetUniform("BlinnPhongShininess", m_ext_rendering_parameters->GetBlinnPhongNshininess());
    cp_shader_rendering->BindUniform("BlinnPhongShininess");

    cp_shader_rendering->SetUniform("BlinnPhongIspecular", m_ext_rendering_parameters->GetLightSourceSpecular());
    cp_shader_rendering->BindUniform("BlinnPhongIspecular");

    cp_shader_rendering->SetUniform("WorldEyePos", camera->GetEye());
    cp_shader_rendering->BindUniform("WorldEyePos");

    cp_shader_rendering->SetUniform("LightSourcePosition", m_ext_rendering_parameters->GetBlinnPhongLightingPosition());
    cp_shader_rendering->BindUniform("LightSourcePosition");

    cp_shader_rendering->BindUniforms();

    gl::Shader::Unbind();
    gl::ExitOnGLError("RayCastingMine: After Update.");
    UpdateMultiDraw(camera);
    return UpdateFirstpass(camera);
}

void RayCastingMine::OneDraw()
{
    m_rdr_frame_to_screen.ClearTexture();

    cp_shader_rendering->Bind();
    m_rdr_frame_to_screen.BindImageTexture();

    cp_shader_rendering->Dispatch();
    gl::ComputeShader::Unbind();

    m_rdr_frame_to_screen.DrawOnePass(render_shader);
}

void RayCastingMine::OneFullDraw()
{
    m_rdr_frame_to_screen.DrawOneFullPass(render_shader, depth_shader_full);
}

void RayCastingMine::MultiDraw()
{
    m_rdr_frame_to_screen.ClearMultiTexture();
    m_rdr_frame_to_screen.BindImageTexture();
    m_rdr_frame_to_screen.DrawMultiPass(render_shader_mutli, first_pass_shader, depth_shader_mutli, numSteps);
}

void RayCastingMine::Redraw()
{
    //OneDraw();
    MultiDraw();
    //OneFullDraw();
}

void RayCastingMine::MultiSampleRedraw()
{
    m_rdr_frame_to_screen.ClearTexture();

    cp_shader_rendering->Bind();
    m_rdr_frame_to_screen.BindImageTexture();

    cp_shader_rendering->Dispatch();
    gl::ComputeShader::Unbind();

    m_rdr_frame_to_screen.DrawMultiSampleHigherResolutionMode();
}

void RayCastingMine::DownScalingRedraw()
{
    m_rdr_frame_to_screen.ClearTexture();

    cp_shader_rendering->Bind();
    m_rdr_frame_to_screen.BindImageTexture();

    cp_shader_rendering->Dispatch();
    gl::ComputeShader::Unbind();

    m_rdr_frame_to_screen.DrawHigherResolutionWithDownScale();
}

void RayCastingMine::UpScalingRedraw()
{
    m_rdr_frame_to_screen.ClearTexture();

    cp_shader_rendering->Bind();
    m_rdr_frame_to_screen.BindImageTexture();

    cp_shader_rendering->Dispatch();
    gl::ComputeShader::Unbind();

    m_rdr_frame_to_screen.DrawLowerResolutionWithUpScale();
}

void RayCastingMine::SetImGuiComponents()
{
    ImGui::Separator();
    ImGui::Text("Step Size: ");
    if (ImGui::DragFloat("###RayCastingMineUIIntegrationStepSize", &m_u_step_size, 0.01f, 0.01f, 100.0f, "%.2f"))
    {
        m_u_step_size = std::max(std::min(m_u_step_size, 100.0f), 0.01f); //When entering with keyboard, ImGui does not take care of this.
        SetOutdated();
    }

    AddImGuiMultiSampleOptions();

    if (m_ext_data_manager->GetCurrentGradientTexture())
    {
        ImGui::Separator();
        if (ImGui::Checkbox("Apply Gradient Shading", &m_apply_gradient_shading))
        {
            // Delete current uniform
            cp_shader_rendering->ClearUniform("TexVolumeGradient");

            if (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture())
            {
                cp_shader_rendering->Bind();
                cp_shader_rendering->SetUniformTexture3D("TexVolumeGradient", m_ext_data_manager->GetCurrentGradientTexture()->GetTextureID(), 3);
                cp_shader_rendering->BindUniform("TexVolumeGradient");
                gl::ComputeShader::Unbind();
            }
            SetOutdated();
        }
        ImGui::Separator();
    }
}

void RayCastingMine::FillParameterSpace(ParameterSpace& pspace)
{
    pspace.ClearParameterDimensions();
    pspace.AddParameterDimension(new ParameterRangeFloat("StepSize", &m_u_step_size, 0.2, 2.0, 0.1));
}

void RayCastingMine::createFirstPass()
{
    glm::vec3 vol_resolution = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetWidth(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetHeight(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetDepth());

    glm::vec3 vol_voxelsize = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleX(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleY(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleZ());

    glm::vec3 vol_aabb = vol_resolution * vol_voxelsize;

    maxLength = glm::length(vol_aabb);
    numSteps = glm::ceil(((double)maxLength) / m_u_step_size);

    render_shader = new gl::PipelineShader();

    render_shader->AddShaderFile(gl::PipelineShader::TYPE::VERTEX, CPPVOLREND_DIR"structured/rcMine/Mine_render.vert");
    render_shader->AddShaderFile(gl::PipelineShader::TYPE::FRAGMENT, CPPVOLREND_DIR"structured/rcMine/Mine_render.frag");

    render_shader->LoadAndLink();
    render_shader->Bind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not create pipeline blend shader...");

    if (m_ext_data_manager->GetCurrentVolumeTexture())
        render_shader->SetUniformTexture3D("TexVolume", m_ext_data_manager->GetCurrentVolumeTexture()->GetTextureID(), 1);
    if (m_glsl_transfer_function)
        render_shader->SetUniformTexture1D("TexTransferFunc", m_glsl_transfer_function->GetTextureID(), 2);
    if (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture())
        render_shader->SetUniformTexture3D("TexVolumeGradient", m_ext_data_manager->GetCurrentGradientTexture()->GetTextureID(), 3);

    render_shader->SetUniform("VolumeGridResolution", vol_resolution);
    render_shader->SetUniform("VolumeVoxelSize", vol_voxelsize);
    render_shader->SetUniform("VolumeGridSize", vol_aabb);
    gl::ExitOnGLError("OOOPS");

    render_shader->BindUniforms();

    glm::mat4 projMat = glm::ortho<float>(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f);
    render_shader->SetUniform("ProjectionMatrixVert", projMat);
    render_shader->BindUniform("ProjectionMatrixVert");
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not bind uniforms...");

    render_shader->Unbind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not unbind pipeline shader...");
}

void RayCastingMine::createMultiRenderPass()
{
    glm::vec3 vol_resolution = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetWidth(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetHeight(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetDepth());

    glm::vec3 vol_voxelsize = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleX(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleY(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleZ());

    glm::vec3 vol_aabb = vol_resolution * vol_voxelsize;

    glm::mat4 projMat = glm::ortho<float>(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f);

    maxLength = glm::length(vol_aabb);
    numSteps = glm::ceil(((double)maxLength) / m_u_step_size);

    printf("Number of steps for multipass: %d\n", numSteps);

    // ############## First shader
    first_pass_shader = new gl::PipelineShader();

    first_pass_shader->AddShaderFile(gl::PipelineShader::TYPE::VERTEX, CPPVOLREND_DIR"structured/rcMine/Mine_render_mult.vert");
    first_pass_shader->AddShaderFile(gl::PipelineShader::TYPE::FRAGMENT, CPPVOLREND_DIR"structured/rcMine/Mine_first_pass.frag");

    first_pass_shader->LoadAndLink();
    first_pass_shader->Bind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not create pipeline blend shader...");

    first_pass_shader->SetUniform("VolumeGridResolution", vol_resolution);
    first_pass_shader->SetUniform("VolumeVoxelSize", vol_voxelsize);
    first_pass_shader->SetUniform("VolumeGridSize", vol_aabb);
    gl::ExitOnGLError("OOOPS");

    first_pass_shader->BindUniforms();

    first_pass_shader->SetUniform("ProjectionMatrixVert", projMat);
    first_pass_shader->BindUniform("ProjectionMatrixVert");
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not bind uniforms...");

    first_pass_shader->Unbind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not unbind pipeline shader...");

    // ############## Render shader
    render_shader_mutli = new gl::PipelineShader();

    render_shader_mutli->AddShaderFile(gl::PipelineShader::TYPE::VERTEX, CPPVOLREND_DIR"structured/rcMine/Mine_render_mult.vert");
    render_shader_mutli->AddShaderFile(gl::PipelineShader::TYPE::FRAGMENT, CPPVOLREND_DIR"structured/rcMine/Mine_render_multi.frag");

    render_shader_mutli->LoadAndLink();
    render_shader_mutli->Bind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not create pipeline blend shader...");

    if (m_ext_data_manager->GetCurrentVolumeTexture())
        render_shader_mutli->SetUniformTexture3D("TexVolume", m_ext_data_manager->GetCurrentVolumeTexture()->GetTextureID(), 1);
    if (m_glsl_transfer_function)
        render_shader_mutli->SetUniformTexture1D("TexTransferFunc", m_glsl_transfer_function->GetTextureID(), 2);
    if (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture())
        render_shader_mutli->SetUniformTexture3D("TexVolumeGradient", m_ext_data_manager->GetCurrentGradientTexture()->GetTextureID(), 3);

    render_shader_mutli->SetUniform("VolumeGridResolution", vol_resolution);
    render_shader_mutli->SetUniform("VolumeVoxelSize", vol_voxelsize);
    render_shader_mutli->SetUniform("VolumeGridSize", vol_aabb);
    gl::ExitOnGLError("OOOPS");

    render_shader_mutli->BindUniforms();

    render_shader_mutli->SetUniform("ProjectionMatrixVert", projMat);
    render_shader_mutli->BindUniform("ProjectionMatrixVert");
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not bind uniforms...");

    render_shader_mutli->Unbind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not unbind pipeline shader...");

    // ############## Depth shader
    depth_shader_mutli = new gl::PipelineShader();

    depth_shader_mutli->AddShaderFile(gl::PipelineShader::TYPE::VERTEX, CPPVOLREND_DIR"structured/rcMine/Mine_render_mult.vert");
    depth_shader_mutli->AddShaderFile(gl::PipelineShader::TYPE::FRAGMENT, CPPVOLREND_DIR"structured/rcMine/Mine_depth_multi.frag");

    depth_shader_mutli->LoadAndLink();
    depth_shader_mutli->Bind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not create pipeline blend shader...");

    depth_shader_mutli->BindUniforms();

    depth_shader_mutli->SetUniform("ProjectionMatrixVert", projMat);
    depth_shader_mutli->BindUniform("ProjectionMatrixVert");
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not bind uniforms...");

    depth_shader_mutli->Unbind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not unbind pipeline shader...");

    // ############## Depth full shader
    depth_shader_full = new gl::PipelineShader();

    depth_shader_full->AddShaderFile(gl::PipelineShader::TYPE::VERTEX, CPPVOLREND_DIR"structured/rcMine/Mine_render_mult.vert");
    depth_shader_full->AddShaderFile(gl::PipelineShader::TYPE::FRAGMENT, CPPVOLREND_DIR"structured/rcMine/Mine_depth_full.frag");

    depth_shader_full->LoadAndLink();
    depth_shader_full->Bind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not create pipeline blend shader...");

   depth_shader_full->SetUniform("VolumeGridResolution", vol_resolution);
   depth_shader_full->SetUniform("VolumeVoxelSize", vol_voxelsize);
   depth_shader_full->SetUniform("VolumeGridSize", vol_aabb);
    gl::ExitOnGLError("OOOPS");

    depth_shader_full->BindUniforms();

    depth_shader_full->SetUniform("ProjectionMatrixVert", projMat);
    depth_shader_full->BindUniform("ProjectionMatrixVert");
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not bind uniforms...");

    depth_shader_full->Unbind();
    gl::ExitOnGLError("vis::RenderFrameToScreen: Could not unbind pipeline shader...");
}

void RayCastingMine::CreateRenderingPass()
{
    glm::vec3 vol_resolution = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetWidth(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetHeight(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetDepth());

    glm::vec3 vol_voxelsize = glm::vec3(m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleX(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleY(),
        m_ext_data_manager->GetCurrentStructuredVolume()->GetScaleZ());

    glm::vec3 vol_aabb = vol_resolution * vol_voxelsize;

    cp_shader_rendering = new gl::ComputeShader();
    cp_shader_rendering->AddShaderFile(CPPVOLREND_DIR"structured/_common_shaders/ray_bbox_intersection.comp");
    cp_shader_rendering->AddShaderFile(CPPVOLREND_DIR"structured/rcMine/ray_marching_Mine.comp");
    //cp_shader_rendering->AddShaderFile(CPPVOLREND_DIR"structured/rc1pass/ray_marching_1p.comp");
    cp_shader_rendering->LoadAndLink();
    cp_shader_rendering->Bind();

    if (m_ext_data_manager->GetCurrentVolumeTexture())
        cp_shader_rendering->SetUniformTexture3D("TexVolume", m_ext_data_manager->GetCurrentVolumeTexture()->GetTextureID(), 1);
    if (m_glsl_transfer_function)
        cp_shader_rendering->SetUniformTexture1D("TexTransferFunc", m_glsl_transfer_function->GetTextureID(), 2);
    if (m_apply_gradient_shading && m_ext_data_manager->GetCurrentGradientTexture())
        cp_shader_rendering->SetUniformTexture3D("TexVolumeGradient", m_ext_data_manager->GetCurrentGradientTexture()->GetTextureID(), 3);

    cp_shader_rendering->SetUniform("VolumeGridResolution", vol_resolution);
    cp_shader_rendering->SetUniform("VolumeVoxelSize", vol_voxelsize);
    cp_shader_rendering->SetUniform("VolumeGridSize", vol_aabb);

    cp_shader_rendering->BindUniforms();
    cp_shader_rendering->Unbind();

    createFirstPass();
    createMultiRenderPass();
}

void RayCastingMine::DestroyRenderingPass()
{
    if (cp_shader_rendering) delete cp_shader_rendering;
    cp_shader_rendering = nullptr;

    gl::ExitOnGLError("Could not destroy shaders");
}

void RayCastingMine::RecreateRenderingPass()
{
    DestroyRenderingPass();
    CreateRenderingPass();

    gl::ExitOnGLError("Could not recreate rendering pass");
}