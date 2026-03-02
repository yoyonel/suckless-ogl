#ifndef RHI_H
#define RHI_H

#include <vulkan/vulkan.h>

typedef struct GLFWwindow GLFWwindow;

typedef enum { API_OPENGL, API_VULKAN } GraphicsAPI;

typedef struct {
	GraphicsAPI api;
	VkInstance instance;
	VkPhysicalDevice physical_device;
	VkDevice device;
} RHIContext;

int rhi_init(RHIContext* ctx, GraphicsAPI api, GLFWwindow* window);
void rhi_cleanup(RHIContext* ctx);

#endif /* RHI_H */
