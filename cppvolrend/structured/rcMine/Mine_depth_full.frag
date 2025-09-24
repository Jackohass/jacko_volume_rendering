#version 430

layout (location = 0) out vec4 FragColor;
layout (depth_less) out float gl_FragDepth;

//layout (rgba32f, binding = 4) uniform image2D dataIn;

uniform vec3 VolumeGridResolution;
uniform vec3 VolumeVoxelSize;
uniform vec3 VolumeGridSize;

uniform float StepSize;

uniform vec3 CameraEye;

uniform vec2 ScreenSize;

uniform mat4 u_CameraLookAt;
uniform mat4 ProjectionMatrix;

uniform float u_TanCameraFovY;
uniform float u_CameraAspectRatio;

//////////////////////////////////////////////////////////////////////////////////////////////////
// From _structured_volume_data/ray_bbox_intersection.frag
struct Ray {
  vec3 Origin;
  vec3 Dir;
};

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

void main (void)
{
    ivec2 storePos = ivec2(gl_FragCoord.xy);

    vec2 fpos = vec2(storePos) + 0.5;

    // Transform fpos from [w, h] to [0, 1] to [-1, 1]
    vec3 VerPos = (vec3(fpos.x / float(ScreenSize.x), fpos.y / float(ScreenSize.y), 0.0) * 2.0) - 1.0;

    // Camera direction
    vec3 camera_dir = normalize(vec3(VerPos.x * u_TanCameraFovY * u_CameraAspectRatio, VerPos.y * u_TanCameraFovY, -1.0) * mat3(u_CameraLookAt));

    // Find Ray Intersection
    Ray r; float tnear, tfar;
    bool inbox = RayAABBIntersection(CameraEye, camera_dir, VolumeGridSize, r, tnear, tfar);
    float D = abs(tfar - tnear);

    float alpha = inbox ? 0.1 : -1.0;
    alpha = D > StepSize ? alpha : -1.0;

    //vec4 dst = vec4(tnear, D, 0.0, alpha);
    //imageStore(dataIn, storePos, dst);
    gl_FragDepth = alpha;
	FragColor = inbox ? vec4(vec3(1.0),1.0) : vec4(vec3(0.0),1.0);
}