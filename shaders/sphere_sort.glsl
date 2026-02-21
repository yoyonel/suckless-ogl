#version 430 core

layout(local_size_x = 256) in;

struct SphereInstance {
    mat4 model;      // 64 bytes
    vec3 albedo;     // 12 bytes (align 16)
    float metallic;  // 4 bytes
    float roughness; // 4 bytes
    float ao;        // 4 bytes
    float padding;   // 4 bytes
    // Total used so far: 64 + 12 + 4 + 4 + 4 + 4 = 92 bytes.
    // C struct is aligned to 64 bytes, so sizeof is 128.
    // We need 36 bytes of padding. 9 floats.
    float _pad[9];
};

layout(std430, binding = 0) buffer DataBuffer {
    SphereInstance data[];
};

uniform int u_stage;       // 0: Prepare, 1: Sort
uniform int u_count;       // Actual count
uniform int u_count_pot;   // Power of two count
uniform vec3 u_cam_pos;

// For sort stage
uniform int u_j;
uniform int u_k;

// Helper to get distance squared
float get_depth(uint idx) {
    if (idx >= u_count) return -1.0; // Push out of bounds items to end (or front?)
    // We want Back-to-Front (Furthest first).
    // So larger depth comes first.
    // If idx >= count, we treat them as depth -infinity so they go to the end?
    // Wait, we want Descending order (Max -> Min).
    // Valid items have depth >= 0.
    // Invalid items should act like depth -1 so they are smaller than any valid depth.

    vec3 pos = vec3(data[idx].model[3]); // Translation column
    float d2 = dot(pos - u_cam_pos, pos - u_cam_pos);
    return d2;
}

void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_count_pot) return;

    // Bitonic Sort
    // We sort indices based on depth.
    // Actually, we are sorting the data array directly for simplicity here,
    // although sorting indices is usually better for large structs.
    // But since we want to avoid Gather on the final draw, swapping structs in SSBO is fine
    // if bandwidth allows. 128 bytes is not too huge.

    // Sort logic:
    // Direction: Descending (Furthest first).

    uint ixj = i ^ u_j;

    if (ixj > i) {
        float d_i = (i < u_count) ? get_depth(i) : -1.0;
        float d_ixj = (ixj < u_count) ? get_depth(ixj) : -1.0;

        bool swap = false;

        // k determines the monotonic sequence direction
        if ((i & u_k) == 0) {
            // Sort Descending
            if (d_i < d_ixj) swap = true;
        } else {
            // Sort Ascending
            if (d_i > d_ixj) swap = true;
        }

        if (swap) {
            SphereInstance temp = data[i];
            data[i] = data[ixj];
            data[ixj] = temp;
        }
    }
}
