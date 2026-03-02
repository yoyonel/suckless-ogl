#include "rhi.h"

#include "log.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>

int rhi_init(RHIContext* ctx, GraphicsAPI api, GLFWwindow* window)
{
	(void)window;
	if (ctx != NULL) {
		ctx->api = api;
		ctx->instance = VK_NULL_HANDLE;
		ctx->physical_device = VK_NULL_HANDLE;
		ctx->device = VK_NULL_HANDLE;
	}

	if (api == API_OPENGL) {
		LOG_INFO("suckless-ogl.rhi",
		         "Initializing OpenGL context wrapper.");
		return 1;
	}

	if (api == API_VULKAN) {
		LOG_INFO("suckless-ogl.rhi", "Initializing Vulkan RHI.");
		VkApplicationInfo app_info = {
		    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		    .pApplicationName = "Suckless-OGL",
		    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		    .pEngineName = "No Engine",
		    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
		    .apiVersion = VK_API_VERSION_1_0};

		uint32_t glfw_extension_count = 0;
		const char** glfw_extensions = NULL;
		glfw_extensions =
		    glfwGetRequiredInstanceExtensions(&glfw_extension_count);

		VkInstanceCreateInfo create_info = {
		    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		    .pApplicationInfo = &app_info,
		    .enabledExtensionCount = glfw_extension_count,
		    .ppEnabledExtensionNames = glfw_extensions};

		/* Request validation layers for debug */
		const char* validation_layers[] = {
		    "VK_LAYER_KHRONOS_validation"};
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = validation_layers;

		if (ctx != NULL) {
			if (vkCreateInstance(&create_info, NULL,
			                     &ctx->instance) != VK_SUCCESS) {
				LOG_ERROR("suckless-ogl.rhi",
				          "Failed to create Vulkan instance.");
				return 0;
			}
		} else {
			return 0;
		}

		uint32_t device_count = 0;
		vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);
		if (device_count == 0) {
			LOG_ERROR("suckless-ogl.rhi",
			          "Failed to find GPUs with Vulkan support.");
			return 0;
		}

		enum { MAX_DEVICES = 8 };
		VkPhysicalDevice devices[MAX_DEVICES] = {VK_NULL_HANDLE};
		if (device_count > MAX_DEVICES) {
			device_count = MAX_DEVICES;
		}
		vkEnumeratePhysicalDevices(ctx->instance, &device_count,
		                           devices);
		if (ctx != NULL) {
			ctx->physical_device =
			    devices[0]; /* Pick first for now */
		}

		float queue_priority = 1.0F;
		VkDeviceQueueCreateInfo queue_create_info = {
		    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		    .queueFamilyIndex =
		        0, /* Assuming 0 supports graphics for now */
		    .queueCount = 1,
		    .pQueuePriorities = &queue_priority};

		VkPhysicalDeviceFeatures device_features = {0};

		VkDeviceCreateInfo device_create_info = {
		    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		    .pQueueCreateInfos = &queue_create_info,
		    .queueCreateInfoCount = 1,
		    .pEnabledFeatures = &device_features,
		    .enabledExtensionCount = 0,
		    .ppEnabledExtensionNames = NULL};

		if (vkCreateDevice(ctx->physical_device, &device_create_info,
		                   NULL, &ctx->device) != VK_SUCCESS) {
			LOG_ERROR("suckless-ogl.rhi",
			          "Failed to create Vulkan logical device.");
			return 0;
		}

		return 1;
	}

	return 0;
}

void rhi_cleanup(RHIContext* ctx)
{
	if (!ctx) {
		return;
	}

	if (ctx->api == API_VULKAN) {
		if (ctx->device != VK_NULL_HANDLE) {
			vkDestroyDevice(ctx->device, NULL);
			ctx->device = VK_NULL_HANDLE;
		}
		if (ctx->instance != VK_NULL_HANDLE) {
			vkDestroyInstance(ctx->instance, NULL);
			ctx->instance = VK_NULL_HANDLE;
		}
	}
}