#version 430

layout (location = 0) out vec4 FragColor;
layout (depth_less) out float gl_FragDepth;

in vec2 color;

uniform sampler2D TexGeneratedFrame;

void main (void)
{
	vec4 frag_color = texture(TexGeneratedFrame, color);
	gl_FragDepth = frag_color.a > 0.99 ? -1.0 : 0.1;
	FragColor = frag_color.a > 0.99 ? vec4(vec3(0.0),1.0) : frag_color;
	//FragColor = frag_color;
}