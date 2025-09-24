#version 430

layout (location = 0) out vec4 FragColor;

layout (depth_any) out float gl_FragDepth;

in vec2 color;

layout (rgba32f, binding = 4) uniform image2D dataIn;
layout (rgba16f, binding = 5) uniform image2D colour;

void main (void)
{
	vec4 frag_color = imageLoad(dataIn, ivec2(gl_FragCoord.xy));
	gl_FragDepth = frag_color.a > 0.99 ? -1.0 : 0.1;
	//gl_FragDepth = frag_color.a > 0.99 ? -1.0 : 0.1;
	//FragColor = frag_color.a > 0.99 ? vec4(vec3(0.0),1.0) : frag_color;
	//FragColor = frag_color.z > 0 ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(vec3(0.0),1.0);
}