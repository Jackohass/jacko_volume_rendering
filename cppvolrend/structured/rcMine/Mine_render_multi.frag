#version 430

layout (location = 0) out vec4 FragColor;

//layout (depth_unchanged) out float gl_FragDepth;
layout(early_fragment_tests) in;

layout (binding = 1) uniform sampler3D TexVolume; 
layout (binding = 2) uniform sampler1D TexTransferFunc;
layout (binding = 3) uniform sampler3D TexVolumeGradient;

uniform vec3 VolumeGridResolution;
uniform vec3 VolumeVoxelSize;
uniform vec3 VolumeGridSize;

uniform vec3 CameraEye;

uniform mat4 u_CameraLookAt;
uniform mat4 ProjectionMatrix;

uniform float u_TanCameraFovY;
uniform float u_CameraAspectRatio;

uniform float StepSize;

uniform vec3 VolumeScales;

uniform int ApplyGradientPhongShading;

uniform float BlinnPhongKa;
uniform float BlinnPhongKd;
uniform float BlinnPhongKs;
uniform float BlinnPhongShininess;

uniform vec3 BlinnPhongIspecular;

uniform vec3 WorldEyePos;
uniform vec3 LightSourcePosition;

uniform int ApplyOcclusion;
uniform int ApplyShadow;

layout (rgba32f, binding = 4) uniform image2D dataIn;
layout (rgba16f, binding = 5) uniform image2D colour;

vec3 ShadeBlinnPhong (vec3 Tpos, vec3 clr)
{
  // Gradient normal
  vec3 gradient_normal =  texture(TexVolumeGradient, Tpos / VolumeGridSize).xyz;
  
  // If is non-zero
  if(gradient_normal != vec3(0, 0, 0))
  {
    vec3 Wpos = Tpos - (VolumeGridSize * 0.5);
    
    gradient_normal      = normalize(gradient_normal);
    
    vec3 light_direction = normalize(LightSourcePosition - Wpos);
    vec3 eye_direction   = normalize(CameraEye - Wpos);
    vec3 halfway_vector  = normalize(eye_direction + light_direction);
  
    //float dot_diff = dot(gradient_normal, light_direction);
    //if (dot_diff < 0) dot_diff = dot(-gradient_normal, light_direction);
    float dot_diff = max(0, dot(gradient_normal, light_direction));

    //float dot_spec = dot(halfway_vector, gradient_normal);
    //if (dot_spec < 0) dot_spec = dot(halfway_vector, -gradient_normal);
    float dot_spec = max(0, dot(halfway_vector, gradient_normal));
   
    clr = 
      // rgb only affects ambient + diffuse
      (clr * (BlinnPhongKa + BlinnPhongKd * dot_diff)) 
      // specular contribution has it's own color
      + BlinnPhongIspecular * BlinnPhongKs * pow(dot_spec, BlinnPhongShininess)
    ;
  }

  return clr;
}

struct Ray {
  vec3 Origin;
  vec3 Dir;
};

//#define USE_TRANSPARENCY
//#define USE_TRANSPARENCY_DS
void main (void)
{
  ivec2 storePos = ivec2(gl_FragCoord.xy);

  vec4 data = imageLoad(dataIn, storePos);
    float tnear = data.x;
    float D = data.y;
    float s = data.z;
    float h = min(StepSize, D - s);
  
  ivec2 size = imageSize(colour);
    // Get screen position [x, y] and consider centering the pixel by + 0.5
    vec2 fpos = vec2(storePos) + 0.5;

    // Transform fpos from [w, h] to [0, 1] to [-1, 1]
    vec3 VerPos = (vec3(fpos.x / float(size.x), fpos.y / float(size.y), 0.0) * 2.0) - 1.0;

    // Camera direction
    vec3 camera_dir = normalize(vec3(VerPos.x * u_TanCameraFovY * u_CameraAspectRatio, VerPos.y * u_TanCameraFovY, -1.0) * mat3(u_CameraLookAt));
    Ray r;
    r.Origin = CameraEye;
    r.Dir = normalize(camera_dir);

    // Find Ray Intersection
    /*Ray r; float tnear, tfar;
    bool inbox = RayAABBIntersection(CameraEye, camera_dir, VolumeGridSize, r, tnear, tfar);
    */
    // If inside volume grid
      // Distance to be evaluated

      // Initialize Transparency and Radiance color
      vec4 dst = imageLoad(colour, storePos);

      // World position at tnear, translated to the volume [0, VolumeGridSize]
      vec3 wld_pos = r.Origin + r.Dir * tnear;
      // Texture position
      vec3 tex_pos = wld_pos + (VolumeGridSize * 0.5);
        
        
        // Get the current step or the remaining interval
        
      
        // Texture position at tnear + (s + h/2)
        vec3 s_tex_pos = tex_pos  + r.Dir * (s + h * 0.5);
      
        // Get normalized density from volume
        float density = texture(TexVolume, s_tex_pos / VolumeGridSize).r;
        
        // Get color from transfer function given the normalized density
        vec4 src = 
          //vec4(density)
          texture(TexTransferFunc, density)
        ;
       
        // if sample is non-transparent
        if(src.a > 0.0)
        {
          // Apply gradient, if enabled
          if(ApplyGradientPhongShading == 1)
            src.rgb = ShadeBlinnPhong(s_tex_pos, src.rgb);
          // Evaluate the current opacity
          src.a = 1.0 - exp(-src.a * h);
          
          // Front-to-back composition
          src.rgb = src.rgb * src.a;
          dst = dst + (1.0 - dst.a) * src;
        }
        // Go to the next interval
        data.z = s + h;
#ifdef USE_TRANSPARENCY
      dst.a = 1.0 - dst.a;
#endif

  FragColor = dst;
  //FragColor = data;
  data.a = dst.a > 0.99 ? 1.0 : data.a;
  data.a = data.z > D ? 1.0 : data.a;
  imageStore(colour, storePos, dst);
  imageStore(dataIn, storePos, data);
  //FragColor = vec4(vec3(0.0), 1.0);
}