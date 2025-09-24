#version 430

layout(location = 0) in vec3 VerPos;

out vec2 color;

uniform mat4 ProjectionMatrixVert;

void main(void)
{
  gl_Position = ProjectionMatrixVert * vec4(VerPos,1.0);
  color = vec2((VerPos.x + 1.0) / 2.0, (VerPos.y + 1.0) / 2.0);
}