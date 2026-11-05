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
#include <map>

const uint32_t WIDTH  = 800;
const uint32_t HEIGHT = 600;

const std::vector<char const *> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

// used to present rendered images to the window surface
const std::vector<const char *> requiredDeviceExtension = {
	vk::KHRSwapchainExtensionName
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
	vk::raii::PhysicalDevice physicalDevice = nullptr;
	vk::raii::Device device = nullptr;
	vk::raii::Queue graphicsQueue = nullptr;

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
		pickPhysicalDevice();
		createLogicalDevice();
	}

	void createLogicalDevice()
	{
		// Get all the queue families supported by the physical device
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// Find the first queue family that supports graphics operations (VK_QUEUE_GRAPHICS_BIT)
		auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) {
			return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
		});

		// Get the index of the graphics queue family
		auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

		// we don't need to check whether the device supports the required extensions because we already did that in
		// isDeviceSuitable()

		float queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
			.queueFamilyIndex = graphicsIndex, 
			.queueCount = 1, 
			.pQueuePriorities = &queuePriority};

		vk::PhysicalDeviceFeatures deviceFeatures;

		// Create a chain of feature structures
		vk::StructureChain<
		    vk::PhysicalDeviceFeatures2,
		    vk::PhysicalDeviceVulkan11Features,
		    vk::PhysicalDeviceVulkan13Features,
		    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
		    featureChain = {
		        {},                                    // vk::PhysicalDeviceFeatures2 (empty for now)
		        {.shaderDrawParameters = true},        // Enable shader draw parameters from Vulkan 1.1
		        {.dynamicRendering = true},            // Enable dynamic rendering from Vulkan 1.3
		        {.extendedDynamicState = true}         // Enable extended dynamic state from the extension
		    };

		vk::DeviceCreateInfo deviceCreateInfo{
		    .pNext = &featureChain.get<vk::PhysicalDeviceFeatures2>(),        // pointer to the first entry in the chain
		    .queueCreateInfoCount    = 1,
		    .pQueueCreateInfos       = &deviceQueueCreateInfo,
		    .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
		    .ppEnabledExtensionNames = requiredDeviceExtension.data()
		};

		device = vk::raii::Device(physicalDevice, deviceCreateInfo);
		graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
	}

	void pickPhysicalDevice()
	{
		auto physicalDevices = instance.enumeratePhysicalDevices();

		if (physicalDevices.empty())
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		for (auto pd : physicalDevices)
		{
			auto deviceProperties = pd.getProperties();
			auto deviceFeatures   = pd.getFeatures();        // like 64-bit floats, multi viewport rendering, etc.	

			if (isDeviceSuitable(pd))
			{
				physicalDevice = pd;
				break;
			}
		}
	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice pd){
		bool apiSupported = pd.getProperties().apiVersion >= vk::ApiVersion13;
		auto queueFamilies    = pd.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(
		    queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		std::vector<const char *> requiredDeviceExtension = {vk::KHRSwapchainExtensionName};
		auto availableDeviceExtensions     = pd.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions = std::ranges::all_of(
		    requiredDeviceExtension, [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
			    return std::ranges::any_of(
			        availableDeviceExtensions, [requiredDeviceExtension](auto const &availableDeviceExtension) {
				        return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
			        });
		    });

		auto features = pd.template getFeatures2<
		    vk::PhysicalDeviceFeatures2,
		    vk::PhysicalDeviceVulkan11Features,
		    vk::PhysicalDeviceVulkan13Features,
		    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures =
		    features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
		    features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		    features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		return (apiSupported 
			&& supportsGraphics 
			&& supportsAllRequiredExtensions 
			&& supportsRequiredFeatures);
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
