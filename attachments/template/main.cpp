#include <memory>
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;

const std::vector<char const *> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	GLFWwindow *window = nullptr;
	vk::raii::Context context;
	vk::raii::Instance instance = nullptr;

	void initWindow()
	{
		glfwInit(); // Initialize the GLFW lib

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Tell GLFW NOT to create an OpeanGL context
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // Disable for now

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void initVulkan()
	{
		createInstance();
	}

	// Bacause GLFW returns a C-style pointer, but we want a C++ vector
	std::vector<const char *> getRequiredInstanceExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		//FYI: adding an int to a pointer automatically gets you the next address, regardless of the stride
		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		return extensions;
	}

	void createInstance()
	{
		// Get the required layers
		std::vector<char const *> requiredLayers;
		if (enableValidationLayers)
		{
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		// We use requiredLayers here instead of simply validationLayers because
		// in the future other kinds of layers may be required and we want to check for those as well.
		auto layerProperties    = context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt = std::ranges::find_if(requiredLayers, [&layerProperties](auto const &requiredLayer) {
			return std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty) {
				return strcmp(layerProperty.layerName, requiredLayer) == 0;
			});
		});
		if (unsupportedLayerIt != requiredLayers.end())
		{
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
		}

		// Get the required extensions.
		auto requiredExtensions = getRequiredInstanceExtensions();

		// Check if the required extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt =
		    std::ranges::find_if(requiredExtensions, [&extensionProperties](auto const &requiredExtension) {
			    return std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {
				    return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
			    });
		    });

		if (unsupportedPropertyIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
		}

		constexpr vk::ApplicationInfo appInfo{
		    .pApplicationName = "Hello Triangle",
		    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		    .pEngineName        = "No Engine",
		    .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
		    .apiVersion         = vk::ApiVersion14,
		};

		vk::InstanceCreateInfo createInfo{
		    .pApplicationInfo        = &appInfo,
		    .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
		    .ppEnabledLayerNames     = requiredLayers.data(),
		    .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
		    .ppEnabledExtensionNames = requiredExtensions.data()};

		instance = vk::raii::Instance(context, createInfo);
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();        // handle window events (like close, resize, inputs, etc.)
		}
	}

	void cleanup()
	{
		glfwDestroyWindow(window);

		glfwTerminate();
	}
};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
