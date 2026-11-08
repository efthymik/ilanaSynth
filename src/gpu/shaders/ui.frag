#version 450
// Pixel maths copied from JUCE's software renderer (juce_RenderingHelpers.h,
// EdgeTableFillers and GradientPixelIterators) so the Vulkan picture matches
// the software one: integer pixel coordinates, the same fixed-point gradient
// indexes and lookup tables, the same bilinear weights and edge rules, and
// PixelARGB::blend's (source * extraAlpha) >> 8 coverage scaling. The fixed
// function blend then adds dest * (1 - source alpha).
layout(location = 0) flat in vec4 vColour;
layout(location = 1) flat in uint vScale;
layout(location = 0) out vec4 outColour;

layout(set = 0, binding = 0) uniform sampler2D image;
layout(std430, set = 0, binding = 1) readonly buffer Lut { uint lut[]; };

layout(push_constant) uniform PC
{
    vec2 targetSize;   // 0
    int mode;          // 8:  0 solid, 1 linear, 2 radial, 3 transformed radial, 4 image, 5 transformed image
    int lutOffset;     // 12
    int lutEntries;    // 16
    int flags;         // 20: 1 tiled, 2 bilinear (quality above low)
    ivec2 offset;      // 24: untransformed image origin
    vec4 m;            // 32
    vec4 n;            // 48
    vec4 g;            // 64
    ivec4 gi;          // 80
} pc;

uvec4 unpackARGB (uint p) { return uvec4 ((p >> 16) & 255u, (p >> 8) & 255u, p & 255u, p >> 24); }

int roundToInt (float v) { return int (floor (v + 0.5)); }

uvec4 fetch (ivec2 p) { return uvec4 (texelFetch (image, p, 0) * 255.0 + 0.5); }

uvec4 gradientAt (int index)
{
    return unpackARGB (lut[pc.lutOffset + clamp (index, 0, pc.lutEntries)]);
}

uvec4 sampleTransformed (ivec2 p, ivec2 size)
{
    bool tiled = (pc.flags & 1) != 0;
    bool bilinear = (pc.flags & 2) != 0;
    // TransformedImageSpanInterpolator: the inverse transform of the pixel
    // (plus 0.5 above low quality), in 1/256 pixel steps, minus 128.
    vec2 s = vec2 (p) + (bilinear ? 0.5 : 0.0);
    vec2 t = vec2 (pc.m.x * s.x + pc.m.y * s.y + pc.m.z,
                   pc.m.w * s.x + pc.n.x * s.y + pc.n.y);
    ivec2 hi = ivec2 (t * 256.0) + (bilinear ? ivec2 (-128) : ivec2 (0));
    ivec2 lo = hi >> 8;
    ivec2 sub = hi & 255;
    ivec2 maxP = size - 1;

    if (tiled)
        lo = ivec2 (lo.x - size.x * int (floor (float (lo.x) / float (size.x))),
                    lo.y - size.y * int (floor (float (lo.y) / float (size.y))));

    if (bilinear)
    {
        bool inX = lo.x >= 0 && lo.x < maxP.x;
        bool inY = lo.y >= 0 && lo.y < maxP.y;

        if (inX && inY)
        {
            uvec4 c = uvec4 (256u * 128u);
            c += uint ((256 - sub.x) * (256 - sub.y)) * fetch (lo);
            c += uint (sub.x * (256 - sub.y)) * fetch (lo + ivec2 (1, 0));
            c += uint (sub.x * sub.y) * fetch (lo + ivec2 (1, 1));
            c += uint ((256 - sub.x) * sub.y) * fetch (lo + ivec2 (0, 1));
            return c >> 16;
        }

        if (inX && ! tiled)
        {
            int y = lo.y < 0 ? 0 : maxP.y;
            uvec4 c = uvec4 (128u) + uint (256 - sub.x) * fetch (ivec2 (lo.x, y)) + uint (sub.x) * fetch (ivec2 (lo.x + 1, y));
            return c >> 8;
        }

        if (! inX && inY && ! tiled)
        {
            int x = lo.x < 0 ? 0 : maxP.x;
            uvec4 c = uvec4 (128u) + uint (256 - sub.y) * fetch (ivec2 (x, lo.y)) + uint (sub.y) * fetch (ivec2 (x, lo.y + 1));
            return c >> 8;
        }
    }

    if (! tiled)
        lo = clamp (lo, ivec2 (0), maxP);

    return fetch (lo);
}

// PixelARGB::blend adds (dest * (256 - srcAlpha)) >> 8, which truncates;
// the blend unit rounds to nearest. Lowering a translucent source's colour
// by just under half a level turns that rounding into truncation wherever
// the colour channel is above zero (flags & 4: a blending draw).
vec4 blendOutput (uvec4 c)
{
    vec4 v = vec4 (c);
    if ((pc.flags & 4) != 0 && c.a < 255u)
        v.rgb = max (v.rgb - 0.49, vec3 (0.0));
    return v / 255.0;
}

void main()
{
    ivec2 p = ivec2 (gl_FragCoord.xy);
    uvec4 src;

    if (pc.mode == 0)
    {
        // already scaled on the CPU
        src = uvec4 (vColour * 255.0 + 0.5);
        outColour = blendOutput (src);
        return;
    }
    else if (pc.mode == 1)
    {
        // GradientPixelIterators::Linear; gi = (vertical/horizontal flags, scale, start), g.x = grad * scale, g.y = yTerm * grad * scale
        int scale = pc.gi.y;
        int index;
        if ((pc.gi.x & 1) != 0)       index = (p.y * scale - pc.gi.z) >> 12;
        else if ((pc.gi.x & 2) != 0)  index = (p.x * scale - pc.gi.z) >> 12;
        else                          index = (p.x * scale - roundToInt (float (p.y) * pc.g.x - pc.g.y)) >> 12;
        src = gradientAt (index);
    }
    else if (pc.mode == 2 || pc.mode == 3)
    {
        // GradientPixelIterators::Radial / TransformedRadial; g = (gx1, gy1, maxDist, invScale)
        float dx, dy;
        if (pc.mode == 2)
        {
            dx = float (p.x) - pc.g.x;
            dy = float (p.y) - pc.g.y;
        }
        else
        {
            dx = pc.m.x * float (p.x) + (pc.m.y * float (p.y) + pc.m.z - pc.g.x);
            dy = pc.m.w * float (p.x) + (pc.n.x * float (p.y) + pc.n.y - pc.g.y);
        }
        float d = dx * dx + dy * dy;
        src = gradientAt (d >= pc.g.z ? pc.lutEntries : min (pc.lutEntries, roundToInt (sqrt (d) * pc.g.w)));
    }
    else if (pc.mode == 4)
    {
        ivec2 size = textureSize (image, 0);
        ivec2 q = p - pc.offset;
        if ((pc.flags & 1) != 0)
            q = ivec2 (q.x - size.x * int (floor (float (q.x) / float (size.x))),
                       q.y - size.y * int (floor (float (q.y) / float (size.y))));
        src = fetch (clamp (q, ivec2 (0), size - 1));
    }
    else
    {
        src = sampleTransformed (p, textureSize (image, 0));
    }

    if (vScale < 256u)
        src = (src * vScale) >> 8;

    outColour = blendOutput (src);
}
