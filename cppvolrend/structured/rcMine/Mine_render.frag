#version 430

//#define OnePass

layout (location = 0) out vec4 FragColor;

//layout (depth_unchanged) out float gl_FragDepth;
layout(early_fragment_tests) in;

in vec2 color;

#ifdef OnePass
layout (rgba32f, binding = 4) uniform image2D dataIn;
#else
#endif

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

//layout (rgba16f, binding = 0) uniform image2D OutputFrag;

uniform vec2 ScreenSize;

//////////////////////////////////////////////////////////////////////////////////////////////////
// From _structured_volume_data/ray_bbox_intersection.frag
struct Ray {
  vec3 Origin;
  vec3 Dir;
};

#ifdef OnePass

#else
bool IntersectBox (Ray r, vec3 boxmin, vec3 boxmax, out float tnear, out float tfar)
{
  vec3 invR = vec3(1.0) / r.Dir;
  
  vec3 tbbmin = invR * (boxmin - r.Origin);
  vec3 tbbmax = invR * (boxmax - r.Origin);
   
  vec3 tmin = min(tbbmin, tbbmax);
  vec3 tmax = max(tbbmin, tbbmax);
  
  tnear = max(max(tmin.x, tmin.y), tmin.z);
  tfar  = min(min(tmax.x, tmax.y), tmax.z);

  return tfar > tnear;
}

bool RayAABBIntersection (vec3 vert_eye, vec3 vert_dir, vec3 vol_scaled_dim,
                          out Ray r, out float rtnear, out float rtfar)
{
  vec3 aabbmin = -vol_scaled_dim * 0.5;
  vec3 aabbmax =  vol_scaled_dim * 0.5;

  r.Origin = vert_eye;
  r.Dir = normalize(vert_dir);
  
  float tnear, tfar;
  bool hit = IntersectBox(r, aabbmin, aabbmax, tnear, tfar);

  tnear = max(tnear, 0.0);

  rtnear = tnear;
  rtfar  = tfar;

  return hit;
}

bool RayAABBIntersection (vec3 vert_eye, vec3 vert_dir, vec3 gridmin, vec3 gridmax,
                          out Ray r, out float rtnear, out float rtfar)
{
  vec3 aabbmin = gridmin;
  vec3 aabbmax = gridmax;

  r.Origin = vert_eye;
  r.Dir = normalize(vert_dir);
  
  float tnear, tfar;
  bool hit = IntersectBox(r, aabbmin, aabbmax, tnear, tfar);

  tnear = max(tnear, 0.0);

  rtnear = tnear;
  rtfar  = tfar;

  return hit;
}
//////////////////////////////////////////////////////////////////////////////////////////////////
#endif
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

//#define USE_TRANSPARENCY
//#define USE_TRANSPARENCY_DS
void main (void)
{
  ivec2 storePos = ivec2(gl_FragCoord.xy);

  vec4 dst;

  FragColor = vec4(vec3(0.2), 1.0);

  /*if (storePos.x < size.x && storePos.y < size.y)
  {*/
    // Get screen position [x, y] and consider centering the pixel by + 0.5
    vec2 fpos = vec2(storePos) + 0.5;

    // Transform fpos from [w, h] to [0, 1] to [-1, 1]
    vec3 VerPos = (vec3(fpos.x / float(ScreenSize.x), fpos.y / float(ScreenSize.y), 0.0) * 2.0) - 1.0;

    // Camera direction
    vec3 camera_dir = normalize(vec3(VerPos.x * u_TanCameraFovY * u_CameraAspectRatio, VerPos.y * u_TanCameraFovY, -1.0) * mat3(u_CameraLookAt));

#ifdef OnePass
    //vec4 data = imageLoad(dataIn, storePos);
    float D = data.y;
    float tnear = data.x;
    Ray r;
    r.Origin = CameraEye;
    r.Dir = normalize(camera_dir);
#else
    // Find Ray Intersection
    Ray r; float tnear, tfar;
    bool inbox = RayAABBIntersection(CameraEye, camera_dir, VolumeGridSize, r, tnear, tfar);
    // Distance to be evaluated
    float D = abs(tfar - tnear);
#endif

    

      // Initialize Transparency and Radiance color
#ifdef USE_TRANSPARENCY
      dst = vec4(vec3(0.0),1.0);
#else
      dst = vec4(0.0);
#endif

      // World position at tnear, translated to the volume [0, VolumeGridSize]
      vec3 wld_pos = r.Origin + r.Dir * tnear;
      // Texture position
      vec3 tex_pos = wld_pos + (VolumeGridSize * 0.5);
      
      // Evaluate from 0 to D...
      for(float s = 0.0; s < D;)
      {
        // Get the current step or the remaining interval
        float h = min(StepSize, D - s);
      
        // Texture position at tnear + (s + h/2)
        vec3 s_tex_pos = tex_pos  + r.Dir * (s + h * 0.5);
      
        // Get normalized density from volume
        float density = texture(TexVolume, s_tex_pos / VolumeGridSize).r;
        
        // Get color from transfer function given the normalized density
        vec4 src = texture(TexTransferFunc, density);
       
        // if sample is non-transparent
        if(src.a > 0.0)
        {
          // Apply gradient, if enabled
          if(ApplyGradientPhongShading == 1)
            src.rgb = ShadeBlinnPhong(s_tex_pos, src.rgb);

#ifdef USE_TRANSPARENCY
  #ifdef USE_TRANSPARENCY_DS
          dst.rgb = dst.rgb + dst.a * src.a * src.rgb * h;
          dst.a = dst.a * exp(-src.a * h);
  #else
          float F = exp(-src.a * h);
          dst.rgb = dst.rgb + dst.a * src.rgb * (1.0 - F);
          dst.a = dst.a * F;
  #endif
          if ((1.0 - dst.a) > 0.99) break;
#else
          // Evaluate the current opacity
          src.a = 1.0 - exp(-src.a * h);
          
          // Front-to-back composition
          src.rgb = src.rgb * src.a;
          dst = dst + (1.0 - dst.a) * src;
          
          // Opacity threshold: 99%
          if (dst.a > 0.99) break;
#endif
        }
        // Go to the next interval
        s = s + h;
      }
#ifdef USE_TRANSPARENCY
      dst.a = 1.0 - dst.a;
#endif
      //imageStore(OutputFrag, storePos, dst);
    /*}
    FragColor = inbox ? dst : vec4(0.3, 0.2, 1, 1.0);
  }
  else
  {
    //FragColor = vec4(0.3, 0.2, 1, 1.0);
  }*/

  FragColor = dst;
  //data.a = 1.0;
  //FragColor = data;
  //vec2 f = storePos;
  //FragColor = vec4(f.x/size.x, f.y/size.y, f.x/f.y, 1.0);
  //FragColor = FragColor.x == 1.0 ? vec4(f.x/size.x, f.y/size.y, f.x/f.y, 1.0) : vec4(vec3(0.0), 0.0);
}