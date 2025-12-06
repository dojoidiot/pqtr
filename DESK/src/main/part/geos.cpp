// geos.cpp - GeoS Geodesic Dome visualization
// Renders a rotating subdivided geodesic dome with smooth color interpolation

#include "geos.hpp"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <random>
#include <array>
#include <vector>
#include <algorithm>
#include <map>

namespace desk {

// ============================================================
// Constants
// ============================================================

constexpr int NUM_DIALS = 17;
constexpr int SUBDIVISIONS = 2;           // 0=20, 1=80, 2=320 triangles
constexpr float ROTATION_SPEED = 0.3f;    // radians per second
constexpr float DRIFT_SPEED = 0.15f;      // dial drift rate
constexpr float DRIFT_RANGE = 0.4f;       // max drift per second
constexpr float PI = 3.14159265359f;

// ============================================================
// State
// ============================================================

static std::array<float, NUM_DIALS> g_dials;          // Dial values [0, 1]
static std::array<float, NUM_DIALS> g_dial_velocity;  // Drift velocity
static float g_rotation_angle = 0.0f;
static float g_tilt_angle = 0.4f;
static double g_last_time = 0.0;
static bool g_initialized = false;
static std::mt19937 g_rng;

// Tuning state
static bool g_tuning = false;
static float g_tune_r = 0.0f;      // Distance from target (0=converged, 1=far)
static float g_tune_theta = 0.0f;  // Direction of error
static float g_tune_loss = 0.0f;   // Current loss value

// ============================================================
// 3D Math
// ============================================================

struct Vec3 {
    float x, y, z;

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

    float length() const { return std::sqrt(x*x + y*y + z*z); }

    float dot(const Vec3& o) const { return x*o.x + y*o.y + z*o.z; }

    Vec3 normalized() const {
        float len = length();
        if (len < 0.0001f) return {0, 0, 0};
        return {x/len, y/len, z/len};
    }

    Vec3 rotateY(float angle) const {
        float c = std::cos(angle), s = std::sin(angle);
        return {x * c + z * s, y, -x * s + z * c};
    }

    Vec3 rotateX(float angle) const {
        float c = std::cos(angle), s = std::sin(angle);
        return {x, y * c - z * s, y * s + z * c};
    }

    bool operator<(const Vec3& o) const {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return z < o.z;
    }
};

static Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

// ============================================================
// Color
// ============================================================

struct Color3 {
    float r, g, b;
    Color3 operator+(const Color3& o) const { return {r+o.r, g+o.g, b+o.b}; }
    Color3 operator*(float s) const { return {r*s, g*s, b*s}; }
};

static Color3 value_to_color3(float v) {
    v = std::clamp(v, 0.0f, 1.0f);
    float hue = v * 270.0f;
    float c = 1.0f;
    float x = c * (1.0f - std::fabs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));

    if (hue < 60.0f)  return {c, x, 0};
    if (hue < 120.0f) return {x, c, 0};
    if (hue < 180.0f) return {0, c, x};
    if (hue < 240.0f) return {0, x, c};
    return {x, 0, c};
}

static ImU32 color3_to_imu32(const Color3& c, float shade = 1.0f) {
    return IM_COL32(
        static_cast<int>(std::clamp(c.r * shade, 0.0f, 1.0f) * 255),
        static_cast<int>(std::clamp(c.g * shade, 0.0f, 1.0f) * 255),
        static_cast<int>(std::clamp(c.b * shade, 0.0f, 1.0f) * 255),
        255
    );
}

// ============================================================
// Geodesic Mesh
// ============================================================

struct Mesh {
    std::vector<Vec3> vertices;
    std::vector<std::array<int, 3>> faces;
};

static constexpr float PHI = 1.618033988749895f;

static Mesh create_icosahedron() {
    Mesh m;

    // 12 vertices
    Vec3 verts[] = {
        {-1,  PHI, 0}, { 1,  PHI, 0}, {-1, -PHI, 0}, { 1, -PHI, 0},
        { 0, -1,  PHI}, { 0,  1,  PHI}, { 0, -1, -PHI}, { 0,  1, -PHI},
        { PHI, 0, -1}, { PHI, 0,  1}, {-PHI, 0, -1}, {-PHI, 0,  1}
    };
    for (auto& v : verts) m.vertices.push_back(v.normalized());

    // 20 faces
    int faces[][3] = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };
    for (auto& f : faces) m.faces.push_back({f[0], f[1], f[2]});

    return m;
}

static int get_or_create_midpoint(Mesh& m, std::map<std::pair<int,int>, int>& cache, int a, int b) {
    auto key = std::make_pair(std::min(a,b), std::max(a,b));
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    Vec3 mid = (m.vertices[a] + m.vertices[b]) * 0.5f;
    int idx = static_cast<int>(m.vertices.size());
    m.vertices.push_back(mid.normalized());
    cache[key] = idx;
    return idx;
}

static void subdivide(Mesh& m) {
    std::map<std::pair<int,int>, int> cache;
    std::vector<std::array<int, 3>> new_faces;

    for (const auto& f : m.faces) {
        int a = get_or_create_midpoint(m, cache, f[0], f[1]);
        int b = get_or_create_midpoint(m, cache, f[1], f[2]);
        int c = get_or_create_midpoint(m, cache, f[2], f[0]);

        new_faces.push_back({f[0], a, c});
        new_faces.push_back({f[1], b, a});
        new_faces.push_back({f[2], c, b});
        new_faces.push_back({a, b, c});
    }
    m.faces = std::move(new_faces);
}

// ============================================================
// Dial Control Points (17 points distributed on sphere)
// ============================================================

static std::array<Vec3, NUM_DIALS> g_dial_positions;

static void init_dial_positions() {
    // Use golden spiral for even distribution
    float golden_angle = PI * (3.0f - std::sqrt(5.0f));

    for (int i = 0; i < NUM_DIALS; i++) {
        float y = 1.0f - (i / float(NUM_DIALS - 1)) * 2.0f;
        float radius = std::sqrt(1.0f - y * y);
        float theta = golden_angle * i;

        g_dial_positions[i] = Vec3{
            std::cos(theta) * radius,
            y,
            std::sin(theta) * radius
        };
    }
}

// Get interpolated color at a point on the sphere
static Color3 get_interpolated_color(const Vec3& pos) {
    // Inverse distance weighting
    float total_weight = 0.0f;
    Color3 result = {0, 0, 0};

    for (int i = 0; i < NUM_DIALS; i++) {
        float dist = (pos - g_dial_positions[i]).length();
        if (dist < 0.001f) {
            return value_to_color3(g_dials[i]);
        }
        // Sharper falloff for more localized colors
        float weight = 1.0f / (dist * dist * dist);
        result = result + value_to_color3(g_dials[i]) * weight;
        total_weight += weight;
    }

    if (total_weight > 0) {
        result = result * (1.0f / total_weight);
    }
    return result;
}

// ============================================================
// Static mesh (built once)
// ============================================================

static Mesh g_mesh;

static void build_mesh() {
    g_mesh = create_icosahedron();
    for (int i = 0; i < SUBDIVISIONS; i++) {
        subdivide(g_mesh);
    }
}

// ============================================================
// Public Interface
// ============================================================

void init_geos() {
    if (g_initialized) return;

    std::random_device rd;
    g_rng.seed(rd());

    init_dial_positions();
    build_mesh();
    randomize_geos_dials();

    // Initialize velocities
    std::uniform_real_distribution<float> vel_dist(-DRIFT_SPEED, DRIFT_SPEED);
    for (int i = 0; i < NUM_DIALS; i++) {
        g_dial_velocity[i] = vel_dist(g_rng);
    }

    g_last_time = glfwGetTime();
    g_initialized = true;
}

void cleanup_geos() {
    g_initialized = false;
}

void randomize_geos_dials() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < NUM_DIALS; i++) {
        g_dials[i] = dist(g_rng);
    }
}

float get_geos_dial(int index) {
    if (index < 0 || index >= NUM_DIALS) return 0.0f;
    return g_dials[index];
}

void set_geos_dial(int index, float value) {
    if (index < 0 || index >= NUM_DIALS) return;
    g_dials[index] = value;
}

void set_geos_tuning(bool active) {
    g_tuning = active;
    if (!active) {
        g_tune_r = 0.0f;
        g_tune_loss = 0.0f;
    }
}

bool is_geos_tuning() {
    return g_tuning;
}

void set_geos_progress(float r, float theta, float loss) {
    g_tune_r = r;
    g_tune_theta = theta;
    g_tune_loss = loss;
}

float get_geos_loss() {
    return g_tune_loss;
}

// ============================================================
// Triangle for depth sorting
// ============================================================

struct RenderTri {
    ImVec2 p[3];
    Color3 c[3];
    Vec3 normal;
    float z_mid;
};

void render_geos_panel() {
    if (!g_initialized) {
        init_geos();
    }

    // Time delta
    double current_time = glfwGetTime();
    float delta = static_cast<float>(current_time - g_last_time);
    g_last_time = current_time;

    // Update rotation (slower when tuning)
    float rot_speed = g_tuning ? ROTATION_SPEED * 0.3f : ROTATION_SPEED;
    g_rotation_angle += delta * rot_speed;

    // Only drift dial values when NOT tuning
    if (!g_tuning) {
        std::uniform_real_distribution<float> drift_dist(-DRIFT_RANGE, DRIFT_RANGE);
        for (int i = 0; i < NUM_DIALS; i++) {
            // Randomly change velocity
            g_dial_velocity[i] += drift_dist(g_rng) * delta;
            g_dial_velocity[i] = std::clamp(g_dial_velocity[i], -DRIFT_SPEED, DRIFT_SPEED);

            // Apply velocity
            g_dials[i] += g_dial_velocity[i] * delta;

            // Bounce off bounds
            if (g_dials[i] <= 0.0f) {
                g_dials[i] = 0.0f;
                g_dial_velocity[i] = std::abs(g_dial_velocity[i]);
            } else if (g_dials[i] >= 1.0f) {
                g_dials[i] = 1.0f;
                g_dial_velocity[i] = -std::abs(g_dial_velocity[i]);
            }
        }
    }

    // Layout
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float size = std::min(avail.x, avail.y) - 40;
    if (size < 50) size = 50;

    float offset_x = (avail.x - size) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 center(canvas_pos.x + size * 0.5f, canvas_pos.y + size * 0.5f);
    float radius = size * 0.42f;

    ImGui::InvisibleButton("##geos_canvas", ImVec2(size, size));
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Background
    draw_list->AddCircleFilled(center, radius + 8, IM_COL32(20, 20, 25, 255), 64);

    // Light direction
    Vec3 light_dir = Vec3{0.4f, 0.6f, 0.6f}.normalized();

    // Build render triangles
    std::vector<RenderTri> tris;
    tris.reserve(g_mesh.faces.size());

    for (const auto& face : g_mesh.faces) {
        RenderTri rt;
        float z_sum = 0;

        Vec3 v[3];
        for (int j = 0; j < 3; j++) {
            Vec3 orig = g_mesh.vertices[face[j]];
            v[j] = orig.rotateY(g_rotation_angle).rotateX(g_tilt_angle);
            z_sum += v[j].z;

            // Get interpolated color at original (unrotated) position
            rt.c[j] = get_interpolated_color(orig);

            rt.p[j].x = center.x + v[j].x * radius;
            rt.p[j].y = center.y - v[j].y * radius;
        }

        Vec3 edge1 = v[1] - v[0];
        Vec3 edge2 = v[2] - v[0];
        rt.normal = cross(edge1, edge2).normalized();
        rt.z_mid = z_sum / 3.0f;

        tris.push_back(rt);
    }

    // Sort back to front
    std::sort(tris.begin(), tris.end(),
              [](const RenderTri& a, const RenderTri& b) { return a.z_mid < b.z_mid; });

    // Draw using primitive API for per-vertex colors (Gouraud shading)
    ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
    for (const auto& tri : tris) {
        if (tri.normal.z < 0) continue;  // Cull back faces

        float diffuse = std::max(0.0f, tri.normal.dot(light_dir));
        float shade = 0.35f + 0.65f * diffuse;

        ImU32 c0 = color3_to_imu32(tri.c[0], shade);
        ImU32 c1 = color3_to_imu32(tri.c[1], shade);
        ImU32 c2 = color3_to_imu32(tri.c[2], shade);

        // Use primitive API for per-vertex colors
        draw_list->PrimReserve(3, 3);
        draw_list->PrimVtx(tri.p[0], uv, c0);
        draw_list->PrimVtx(tri.p[1], uv, c1);
        draw_list->PrimVtx(tri.p[2], uv, c2);

        // Subtle edge
        draw_list->AddTriangle(tri.p[0], tri.p[1], tri.p[2], IM_COL32(0, 0, 0, 30), 0.5f);
    }

    // Draw optimizer position when tuning
    if (g_tuning) {
        // Target at center (north pole projected)
        draw_list->AddCircleFilled(center, 6, IM_COL32(0, 255, 0, 200), 16);

        // Current position based on r and theta
        float px = center.x + g_tune_r * radius * 0.8f * std::cos(g_tune_theta);
        float py = center.y - g_tune_r * radius * 0.8f * std::sin(g_tune_theta);

        // Trail line from center to position
        draw_list->AddLine(center, ImVec2(px, py), IM_COL32(255, 100, 100, 150), 2.0f);

        // Current position dot
        ImU32 dot_color = IM_COL32(255, 50, 50, 255);
        if (g_tune_r < 0.1f) dot_color = IM_COL32(50, 255, 50, 255);  // Green when close
        else if (g_tune_r < 0.3f) dot_color = IM_COL32(255, 255, 50, 255);  // Yellow
        draw_list->AddCircleFilled(ImVec2(px, py), 8, dot_color, 16);
        draw_list->AddCircle(ImVec2(px, py), 8, IM_COL32(255, 255, 255, 200), 16, 1.5f);

        // Loss percentage text
        char loss_text[32];
        snprintf(loss_text, sizeof(loss_text), "%.1f%%", g_tune_loss * 100.0f);
        ImVec2 text_size = ImGui::CalcTextSize(loss_text);
        draw_list->AddText(
            ImVec2(center.x - text_size.x * 0.5f, center.y + radius + 15),
            IM_COL32(255, 255, 255, 255), loss_text
        );
    }

    // Randomize button (hidden when tuning)
    if (!g_tuning) {
        ImGui::SetCursorPosX((avail.x - 100) * 0.5f);
        if (ImGui::Button("Randomize", ImVec2(100, 0))) {
            randomize_geos_dials();
            std::uniform_real_distribution<float> vel_dist(-DRIFT_SPEED, DRIFT_SPEED);
            for (int i = 0; i < NUM_DIALS; i++) {
                g_dial_velocity[i] = vel_dist(g_rng);
            }
        }
    }
}

} // namespace desk
