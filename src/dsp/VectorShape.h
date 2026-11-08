#pragma once

// The vector pad's shapes (ilana, UI review 29: "a triangle if only using 3
// oscs, and a slider if two"): one corner per oscillator, 2 to 6 corners.
//
//   Slider    2 corners, A left, B right (X only).
//   Triangle  3 corners, A top, then clockwise.
//   Square    4 corners, A top left, B top right, C bottom left, D bottom
//             right: the pad every older patch has, with its bilinear mix,
//             bit for bit.
//   Pentagon  5 corners, A top, then clockwise.
//   Hexagon   6 corners, A top, then clockwise.
//
// Positions are the pad's X and Y (0..1, Y up). A shape's corners are a
// regular polygon stretched to fill the unit square, so every shape uses the
// whole pad. A point outside the polygon counts as the nearest point on its
// edge. The mix is equal power: each corner's gain is the square root of its
// generalised barycentric (Wachspress) coordinate, so the squared gains sum
// to one, as the square's bilinear gains do (and on a square, Wachspress
// coordinates are the bilinear ones).

#include <array>
#include <cmath>

namespace VectorShape
{
enum Shape { slider = 0, triangle, square, pentagon, hexagon, numShapes };

constexpr int maxCorners = 6;

inline const char* const names[numShapes] { "Slider", "Triangle", "Square", "Pentagon", "Hexagon" };

inline int cornerCount (int shape)
{
    return shape <= slider ? 2 : shape >= hexagon ? 6 : shape + 2;
}

struct Point { float x = 0.0f, y = 0.0f; };

// The corners of a shape, in corner order (A, B, ...), in pad units.
inline std::array<Point, maxCorners> corners (int shape)
{
    std::array<Point, maxCorners> out {};
    if (shape == slider)
    {
        out[0] = { 0.0f, 0.5f };
        out[1] = { 1.0f, 0.5f };
        return out;
    }
    if (shape == square)
    {
        out[0] = { 0.0f, 1.0f };
        out[1] = { 1.0f, 1.0f };
        out[2] = { 0.0f, 0.0f };
        out[3] = { 1.0f, 0.0f };
        return out;
    }

    const auto n = cornerCount (shape);
    float minX = 1.0e9f, maxX = -1.0e9f, minY = 1.0e9f, maxY = -1.0e9f;
    for (int i = 0; i < n; ++i)
    {
        // From the top, clockwise.
        const auto angle = 1.5707963267948966 - 6.283185307179586 * (double) i / (double) n;
        out[(size_t) i] = { (float) std::cos (angle), (float) std::sin (angle) };
        minX = std::fmin (minX, out[(size_t) i].x);
        maxX = std::fmax (maxX, out[(size_t) i].x);
        minY = std::fmin (minY, out[(size_t) i].y);
        maxY = std::fmax (maxY, out[(size_t) i].y);
    }
    for (int i = 0; i < n; ++i)
    {
        auto& p = out[(size_t) i];
        p.x = std::fabs (p.x) < 1.0e-6f ? 0.5f : (p.x - minX) / (maxX - minX);
        p.y = (p.y - minY) / (maxY - minY);
    }
    return out;
}

// The point where every corner sounds about evenly: the average of the
// corners (the centre for the slider and the square).
inline Point balance (int shape)
{
    const auto n = cornerCount (shape);
    const auto c = corners (shape);
    Point sum;
    for (int i = 0; i < n; ++i)
    {
        sum.x += c[(size_t) i].x;
        sum.y += c[(size_t) i].y;
    }
    return { sum.x / (float) n, sum.y / (float) n };
}

// The corners in the order they go round the edge (the square's are stored
// TL, TR, BL, BR, so its ring is A, B, D, C).
inline std::array<int, maxCorners> ring (int shape)
{
    if (shape == square)
        return { 0, 1, 3, 2, 0, 0 };
    return { 0, 1, 2, 3, 4, 5 };
}

inline float cross (Point o, Point a, Point b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// The point itself where it lies inside the shape; else the nearest point on
// its edge. (A slider keeps X and centres Y.)
inline Point clamp (int shape, Point p)
{
    p.x = std::fmin (1.0f, std::fmax (0.0f, p.x));
    p.y = std::fmin (1.0f, std::fmax (0.0f, p.y));
    if (shape == slider)
        return { p.x, 0.5f };
    if (shape == square)
        return p;

    const auto n = cornerCount (shape);
    const auto c = corners (shape);
    const auto order = ring (shape);
    auto inside = true;
    for (int i = 0; i < n && inside; ++i)
        inside = cross (c[(size_t) order[(size_t) i]], c[(size_t) order[(size_t) ((i + 1) % n)]], p) <= 0.0f; // clockwise: inside is right
    if (inside)
        return p;

    auto best = p;
    auto bestDistance = 1.0e9f;
    for (int i = 0; i < n; ++i)
    {
        const auto a = c[(size_t) order[(size_t) i]], b = c[(size_t) order[(size_t) ((i + 1) % n)]];
        const auto dx = b.x - a.x, dy = b.y - a.y;
        auto t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy);
        t = std::fmin (1.0f, std::fmax (0.0f, t));
        const Point q { a.x + t * dx, a.y + t * dy };
        const auto distance = (q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = q;
        }
    }
    return best;
}

// Each corner's gain at a point (equal power), corner order A, B, ...;
// unused corners are zero.
inline std::array<float, maxCorners> gains (int shape, Point p)
{
    std::array<float, maxCorners> out {};
    if (shape == square)
    {
        // The pad every older patch has (IlanaSynthAudioProcessor::vectorWeights, unchanged).
        const auto x = std::fmin (1.0f, std::fmax (0.0f, p.x));
        const auto y = std::fmin (1.0f, std::fmax (0.0f, p.y));
        out[0] = std::sqrt ((1.0f - x) * y);
        out[1] = std::sqrt (x * y);
        out[2] = std::sqrt ((1.0f - x) * (1.0f - y));
        out[3] = std::sqrt (x * (1.0f - y));
        return out;
    }

    p = clamp (shape, p);
    if (shape == slider)
    {
        out[0] = std::sqrt (1.0f - p.x);
        out[1] = std::sqrt (p.x);
        return out;
    }

    // Wachspress coordinates in their product form (no division, so a point on
    // an edge or a corner is exact): w_i = C_i * product of A_j over the edges
    // j that don't touch corner i, where A_j is the area of (p, edge j) and C_i
    // the area of the corner's own triangle with its two neighbours.
    const auto n = cornerCount (shape);
    const auto c = corners (shape);
    const auto order = ring (shape);
    std::array<float, maxCorners> edgeArea {};
    for (int j = 0; j < n; ++j)
        edgeArea[(size_t) j] = std::fmax (0.0f, cross (p, c[(size_t) order[(size_t) ((j + 1) % n)]], c[(size_t) order[(size_t) j]]));

    std::array<double, maxCorners> w {};
    double total = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const auto previous = c[(size_t) order[(size_t) ((i + n - 1) % n)]];
        const auto here = c[(size_t) order[(size_t) i]];
        const auto next = c[(size_t) order[(size_t) ((i + 1) % n)]];
        double value = std::fabs (cross (previous, here, next));
        for (int j = 0; j < n; ++j)
            if (j != i && j != (i + n - 1) % n) // edge j runs from ring corner j to j + 1
                value *= edgeArea[(size_t) j];
        w[(size_t) i] = value;
        total += value;
    }
    for (int i = 0; i < n; ++i)
        out[(size_t) order[(size_t) i]] = total > 0.0 ? (float) std::sqrt (w[(size_t) i] / total) : (i == 0 ? 1.0f : 0.0f);
    return out;
}
} // namespace VectorShape
