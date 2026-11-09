#version 450
// One instance per edge-table span: a rectangle in target pixels, a packed
// colour (solid fills: the finished premultiplied colour) and a coverage
// scale e (0..256; 256 leaves the source as it is). Six vertices make the
// rectangle's two triangles.
layout(location = 0) in ivec4 inRect;     // x, y, w, h
layout(location = 1) in vec4 inColour;    // bytes B, G, R, A of a juce PixelARGB
layout(location = 2) in uint inScale;

layout(location = 0) flat out vec4 vColour;
layout(location = 1) flat out uint vScale;

layout(push_constant) uniform PC
{
    vec2 targetSize;
} pc;

void main()
{
    const vec2 corners[6] = vec2[6] (vec2 (0, 0), vec2 (1, 0), vec2 (0, 1),
                                     vec2 (1, 0), vec2 (1, 1), vec2 (0, 1));
    vec2 pos = vec2 (inRect.xy) + corners[gl_VertexIndex] * vec2 (inRect.zw);
    gl_Position = vec4 (pos / pc.targetSize * 2.0 - 1.0, 0.0, 1.0);
    vColour = inColour.zyxw;
    vScale = inScale;
}
