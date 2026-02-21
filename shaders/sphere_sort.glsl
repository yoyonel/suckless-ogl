#version 430 core

layout(local_size_x = 1024) in;

struct SphereInstance {
	mat4 model;       // 64 bytes
	vec3 albedo;      // 12 bytes (align 16)
	float metallic;   // 4 bytes
	float roughness;  // 4 bytes
	float ao;         // 4 bytes
	float padding;    // 4 bytes
	float _pad[9];    // 36 bytes
};

struct Entry {
	int index;
	float depth;
};

layout(std430, binding = 0) buffer DataBuffer
{
	SphereInstance instances[];
};

layout(std430, binding = 1) buffer EntryBuffer
{
	Entry entries[];
};

layout(std430, binding = 2) buffer SortedBuffer
{
	SphereInstance sorted_instances[];
};

uniform int u_stage;      // 0: Prepare, 1: Sort, 2: Permute, 3: Single-Pass
uniform int u_count;      // Actual count
uniform int u_count_pot;  // Power of two count
uniform vec3 u_cam_pos;

// For sort stage
uniform int u_j;
uniform int u_k;

shared Entry s_entries[1024];

void main()
{
	uint i = gl_GlobalInvocationID.x;

	if (u_stage == 3) {
		// --- SINGLE PASS SHARED MEMORY MODE (For count <= 1024) ---
		// 1. Load / Prepare
		if (i < u_count_pot) {
			if (i < u_count) {
				vec3 pos = vec3(instances[i].model[3]);
				float d2 =
				    dot(pos - u_cam_pos, pos - u_cam_pos);
				s_entries[i].depth = d2;
				s_entries[i].index = int(i);
			} else {
				s_entries[i].depth = -1.0;
				s_entries[i].index = -1;
			}
		}
		barrier();

		// 2. Bitonic Sort in Shared Memory
		for (uint k = 2; k <= u_count_pot; k <<= 1) {
			for (uint j = k >> 1; j > 0; j >>= 1) {
				uint ixj = i ^ j;
				if (ixj > i && ixj < u_count_pot) {
					float d_i = s_entries[i].depth;
					float d_ixj = s_entries[ixj].depth;

					bool swap = false;
					if ((i & k) == 0) {
						if (d_i < d_ixj)
							swap = true;
					} else {
						if (d_i > d_ixj)
							swap = true;
					}

					if (swap) {
						Entry temp = s_entries[i];
						s_entries[i] = s_entries[ixj];
						s_entries[ixj] = temp;
					}
				}
				barrier();
			}
		}

		// 3. Permute / Final Writeback
		if (i < u_count) {
			int original_idx = s_entries[i].index;
			if (original_idx >= 0 && original_idx < u_count) {
				sorted_instances[i] = instances[original_idx];
			}
		}
		return;
	}

	// --- MULTI-PASS GLOBAL MEMORY MODE ---
	if (u_stage == 0) {
		if (i >= u_count_pot)
			return;
		// PREPARE stage: Calculate depth for valid items once
		if (i < u_count) {
			vec3 pos =
			    vec3(instances[i].model[3]);  // Translation column
			float d2 = dot(pos - u_cam_pos, pos - u_cam_pos);
			entries[i].depth = d2;
			entries[i].index = int(i);
		} else {
			// Out of bounds items get minimal depth
			entries[i].depth = -1.0;
			entries[i].index = -1;
		}
	} else if (u_stage == 1) {
		if (i >= u_count_pot)
			return;
		// SORT stage: Bitonic Sort on entries
		uint ixj = i ^ u_j;

		if (ixj > i) {
			float d_i = entries[i].depth;
			float d_ixj = entries[ixj].depth;

			bool swap = false;
			if ((i & u_k) == 0) {
				if (d_i < d_ixj)
					swap = true;
			} else {
				if (d_i > d_ixj)
					swap = true;
			}

			if (swap) {
				Entry temp = entries[i];
				entries[i] = entries[ixj];
				entries[ixj] = temp;
			}
		}
	} else if (u_stage == 2) {
		if (i >= u_count)
			return;
		// PERMUTE stage: Reorder instances based on sorted entries
		int original_idx = entries[i].index;
		if (original_idx >= 0 && original_idx < u_count) {
			sorted_instances[i] = instances[original_idx];
		}
	}
}
