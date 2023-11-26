#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <video.hpp>
#include <string.h>
#include <cstdint>   // Necessary for uint32_t
#include <limits>    // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <set>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
static bool isDebugBuild           = true;
static bool enableValidationLayers = true;

const std::vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"};

const std::vector<const char*> deviceExtensions = {};

namespace NextVideo {
static const char* engineName      = "No engine";
static const char* applicationName = "Test application";

ENGINE_API const char* readFile(const char* path);

static VkFormat gDefaultWriteFormat;
static VkFormat gDefaultDepthWriteFormat;

struct VKRenderer : public IRenderer {

  struct IO {
    struct buffer {
      char*         data;
      unsigned long count;
    };
  };

  IO io;
  template <typename T>
  using ptr = std::unique_ptr<T>;


  template <typename T, typename... Args>
  ptr<T> make_ptr(Args&&... args) { return std::make_unique<T>(std::forward<Args>(args)...); }

  ENGINE_API IO::buffer IO_readFile(const char* path) {
    const char* data = readFile(path);
    VERIFY(data != nullptr, "[IO] Error reading data\n");
    return {(char*)data, strlen(data)};
  }

  /* VK Util */

  struct Image {
    VkImage     image;
    VkFormat    format;
    VkImageView defaultView;
    VkDevice    owner;

    VkImageView getDefaultView() {
      if (defaultView == VK_NULL_HANDLE) {

        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;

        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format   = format;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        HARD_CHECK(vkCreateImageView(owner, &createInfo, nullptr, &defaultView) == VK_SUCCESS, "Error creating image view\n");
        LOG("[VK] Image view created\n");
      }
      return defaultView;
    }


    Image(VkImage image, VkFormat format, VkDevice owner) {
      this->image  = image;
      this->owner  = owner;
      this->format = format;
    }

    ~Image() {
      LOG("[VK] Image destroyed");
    }
  };

  enum ShaderStages {
    VERTEX = 0,
    FRAGMENT,
    SHADER_STAGES_COUNT
  };


  struct VertexAttribute {
    int size;
  };


  static VkAttachmentDescription colorAttachmentSwapChain() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format  = gSwapChainFormatColor;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return {colorAttachment};
  }

  static VkAttachmentDescription depthAttachmentSwapChain() {}


  struct SubpassDescReference {
    int           index;
    VkImageLayout layout;
  };
  struct SubpassDesc {
    std::vector<SubpassDescReference> colorIndices;
    int                               depth;
  };

  struct Subpass {
    std::vector<std::vector<VkAttachmentReference>> references;
    std::vector<VkSubpassDescription>               subpasses;

    Subpass(int size) {
      subpasses.resize(size);
      references.resize(size);
    }
  };

  static SubpassDesc defaultSubpass() { return {{{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}}, 0}; }


  struct RenderPassDesc {
    std::vector<VkAttachmentDescription> colorAttachments = {colorAttachmentSwapChain()};
    VkAttachmentDescription              depthAttachment  = depthAttachmentSwapChain();
    std::vector<SubpassDesc>             subpasses        = {defaultSubpass()};


    Subpass getSubpasses() {
      Subpass result(subpasses.size());
      for (int i = 0; i < subpasses.size(); i++) {
        for (int j = 0; j < subpasses[i].colorIndices.size(); j++) {
          VkAttachmentReference reference;
          reference.attachment = subpasses[i].colorIndices[j].index;
          reference.layout     = subpasses[i].colorIndices[j].layout;
          result.references[i].push_back(reference);
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = subpasses[i].colorIndices.size();
        subpass.pColorAttachments    = result.references[i].data();

        result.subpasses.push_back(subpass);
      }
      return result;
    }
  };

  struct RenderPass {
    VkRenderPass renderPass;

    RenderPass(RenderPassDesc _desc) { this->desc = _desc; }
    RenderPassDesc desc;
  };

  struct PipelineLayout {
    VkPipelineLayout layout;
  };

  struct PipelineDesc {
    IO::buffer                   shaders[5];
    std::vector<VertexAttribute> vertexAttribs;
    RenderPass*                  renderPass = nullptr;
    PipelineLayout*              layout     = nullptr;

    // desc
    VkPipelineInputAssemblyStateCreateInfo inputAssembly_ci = trianglesAssembly();
    VkPipelineRasterizationStateCreateInfo rasterizer_ci    = fill();
    VkPipelineMultisampleStateCreateInfo   multisample_ci   = msaa();
    VkPipelineColorBlendAttachmentState    colorBlend[4]    = {colorBlendingDefault()};

    static VkPipelineMultisampleStateCreateInfo msaa() {

      VkPipelineMultisampleStateCreateInfo multisampling{};
      multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
      multisampling.sampleShadingEnable   = VK_FALSE;
      multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
      multisampling.minSampleShading      = 1.0f;
      multisampling.pSampleMask           = nullptr;
      multisampling.alphaToCoverageEnable = VK_FALSE;
      multisampling.alphaToOneEnable      = VK_FALSE;
      return multisampling;
    }
    static VkPipelineRasterizationStateCreateInfo fill() {
      VkPipelineRasterizationStateCreateInfo rasterizer{};
      rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
      rasterizer.depthClampEnable        = VK_FALSE;
      rasterizer.rasterizerDiscardEnable = VK_FALSE;
      rasterizer.rasterizerDiscardEnable = VK_FALSE;
      rasterizer.lineWidth               = 1.0f;
      rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
      rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
      rasterizer.depthBiasEnable         = VK_FALSE;
      //Optional
      rasterizer.depthBiasConstantFactor = 0.0f;
      rasterizer.depthBiasClamp          = 0.0f;
      rasterizer.depthBiasSlopeFactor    = 0.0f;
      return rasterizer;
    }
    static VkPipelineInputAssemblyStateCreateInfo trianglesAssembly() {

      VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
      inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
      inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      inputAssembly.primitiveRestartEnable = VK_FALSE;
      return inputAssembly;
    }
    VkPipelineColorBlendStateCreateInfo colorBlending_ci() {

      VkPipelineColorBlendStateCreateInfo colorBlending{};
      colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      colorBlending.logicOpEnable     = VK_FALSE;
      colorBlending.logicOp           = VK_LOGIC_OP_COPY; // Optional
      colorBlending.attachmentCount   = 1;
      colorBlending.pAttachments      = colorBlend;
      colorBlending.blendConstants[0] = 0.0f; // Optional
      colorBlending.blendConstants[1] = 0.0f; // Optional
      colorBlending.blendConstants[2] = 0.0f; // Optional
      colorBlending.blendConstants[3] = 0.0f; // Optional
      return colorBlending;
    }

    static VkPipelineColorBlendAttachmentState colorBlendingDefault() {
      VkPipelineColorBlendAttachmentState colorBlendAttachment;
      colorBlendAttachment.blendEnable         = VK_TRUE;
      colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
      colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
      colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
      colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
      colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
      colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
      return colorBlendAttachment;
    }
  };

  PipelineLayout* pipelineLayoutCreate(VkDevice device) {

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 0;       // Optional
    pipelineLayoutInfo.pSetLayouts            = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0;       // Optional
    pipelineLayoutInfo.pPushConstantRanges    = nullptr; // Optional

    VkPipelineLayout pipelineLayout;
    HARD_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS, "Error creating pipeline layout");
    LOG("[VK] Layout created\n");
    return new PipelineLayout{pipelineLayout};
  }

  RenderPass* renderPassCreate(RenderPassDesc desc, VkDevice device) {
    RenderPass* renderPass = new RenderPass(desc);

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = desc.colorAttachments.size();
    renderPassInfo.pAttachments    = desc.colorAttachments.data();

    Subpass subpasses           = desc.getSubpasses();
    renderPassInfo.subpassCount = subpasses.subpasses.size();
    renderPassInfo.pSubpasses   = subpasses.subpasses.data();

    HARD_CHECK(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass->renderPass) == VK_SUCCESS, "Could not create render pass!\n");
    LOG("[VK] Render pass created\n");
    return renderPass;
  }

  struct PipelineDynamicState {
    VkViewport viewport{};
    VkRect2D   scissor{};
  };

  struct Pipeline {
    VkPipeline pipeline;
    VkDevice   owner;

    Pipeline(PipelineDesc desc) { this->desc = desc; }
    ~Pipeline() {
      vkDestroyPipeline(owner, pipeline, nullptr);
    }

    PipelineDesc desc;
  };

  VkShaderModule shaderModuleCreate(IO::buffer buffer, VkDevice device) {
    if (buffer.data == nullptr) return VK_NULL_HANDLE;
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.count;
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(buffer.data);
    VkShaderModule result;
    HARD_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &result) == VK_SUCCESS, "Could not create shader module\n");
    return result;
  }

  Pipeline* pipelineCreate(PipelineDesc desc, VkDevice device) {
    Pipeline* pip = new Pipeline(desc);

    VERIFY(desc.renderPass != nullptr, "[VK] Error creating pipeline, invalid renderPass\n");
    VERIFY(desc.layout != nullptr, "[VK] Error creating pipeline, invalid layout\n");

    bool                            presentShaders[SHADER_STAGES_COUNT];
    VkShaderModule                  shaderModules[SHADER_STAGES_COUNT];
    VkPipelineShaderStageCreateInfo shaderStages_ci[SHADER_STAGES_COUNT];


    static VkShaderStageFlagBits flags[] = {
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_SHADER_STAGE_FRAGMENT_BIT};
    int baseIdx = 0;

    for (int i = 0; i < SHADER_STAGES_COUNT; i++) {
      if ((shaderModules[i] = shaderModuleCreate(desc.shaders[i], device)) != VK_NULL_HANDLE) {
        presentShaders[i] = true;
        VkPipelineShaderStageCreateInfo shaderStageInfo{};
        shaderStageInfo.sType      = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage      = flags[i];
        shaderStageInfo.module     = shaderModules[i];
        shaderStageInfo.pName      = "main";
        shaderStages_ci[baseIdx++] = shaderStageInfo;
      };
    }
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 0;
    vertexInputInfo.pVertexBindingDescriptions      = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions    = nullptr;

    const static VkDynamicState dynamicStates[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = sizeof(dynamicStates) / sizeof(VkDynamicState);
    dynamicState.pDynamicStates    = dynamicStates;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = baseIdx;
    pipelineInfo.pStages    = shaderStages_ci;


    VkPipelineColorBlendStateCreateInfo colorBlending = desc.colorBlending_ci();
    pipelineInfo.pVertexInputState                    = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState                  = &desc.inputAssembly_ci;
    pipelineInfo.pViewportState                       = &viewportState;
    pipelineInfo.pRasterizationState                  = &rasterizer;
    pipelineInfo.pMultisampleState                    = &desc.multisample_ci;
    pipelineInfo.pDepthStencilState                   = nullptr; // Optional
    pipelineInfo.pColorBlendState                     = &colorBlending;
    pipelineInfo.pDynamicState                        = &dynamicState;

    pipelineInfo.layout     = desc.layout->layout;
    pipelineInfo.renderPass = desc.renderPass->renderPass;
    pipelineInfo.subpass    = 0;

    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex  = -1;             // Optional
                                                      //

    VkPipeline graphicsPipeline;
    HARD_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) == VK_SUCCESS, "Could not create pipeline\n");
    LOG("[VK] Pipeline created\n");
    pip->pipeline = graphicsPipeline;
    pip->owner    = device;
    return pip;
  }

  /* VK CALLBACKS */

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    dprintf(2, "validation layer: %s\n", pCallbackData->pMessage);
    return VK_FALSE;
  }

  /** VK Validation layers */

  bool validationLayersSupport(const std::vector<const char*>& requested) {

    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    for (const char* layerName : requested) {
      bool layerFound = false;
      for (const auto& layerProperties : availableLayers) {
        if (strcmp(layerName, layerProperties.layerName) == 0) {
          layerFound = true;
          break;
        }
      }
      if (!layerFound) {
        return false;
      }
    }
    return true;
  }
  /** VK Extensions */

  std::vector<const char*> getRequiredExtensions() {
    SurfaceExtensions        extensions = _desc.surface->getExtensions();
    std::vector<const char*> result;
    for (int i = 0; i < extensions.count; i++) result.push_back(extensions.names[i]);

    if (enableValidationLayers) {
      result.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return result;
  }

  /** VK INSTANCE **/
  void instanceCreate() {
    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = applicationName;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = engineName;
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo pCreateInfo{};
    pCreateInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    pCreateInfo.pApplicationInfo = &appInfo;

    auto extensions                     = getRequiredExtensions();
    pCreateInfo.enabledExtensionCount   = extensions.size();
    pCreateInfo.ppEnabledExtensionNames = extensions.data();
    pCreateInfo.enabledLayerCount       = 0;

    if (isDebugBuild) {
      for (int i = 0; i < extensions.size(); i++) {
        LOG("[VK] Asking for extension %d %s\n", i, extensions[i]);
      }
    }
    if (enableValidationLayers) {
      HARD_CHECK(validationLayersSupport(validationLayers), "Validation layers requested but not supported\n");
      pCreateInfo.ppEnabledLayerNames = validationLayers.data();
      pCreateInfo.enabledLayerCount   = validationLayers.size();
      LOG("[VK] Validation layers requested\n");
    }

    HARD_CHECK(vkCreateInstance(&pCreateInfo, mAllocator, &mInstance) == VK_SUCCESS, "Error creating instance");
    LOG("[VK] Instance created\n");
  }

  void instanceDestroy() {
    vkDestroyInstance(mInstance, mAllocator);
  }

  /** VK INSTANCE END */


  /** VK Messenger */
  void setupMessenger() {
  }

  /** VK DEVICES*/

  std::vector<VkPhysicalDevice> getDevices() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());
    return devices;
  }

  bool deviceIsDiscrete(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures   deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
  }

  int deviceRate(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures   deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    int score = 0;
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
    score += deviceProperties.limits.maxImageDimension2D;
    return score;
  }

  void deviceLog(VkPhysicalDevice device) {

    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures   deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    LOG("[VK] Device %s vendor %d\n", deviceProperties.deviceName, deviceProperties.vendorID);
  }

  VkPhysicalDevice pickPhysicalDevice() {
    auto devices = getDevices();
    HARD_CHECK(devices.size() > 0, "[VK] No devices\n");
    std::vector<int> scores(devices.size());
    std::vector<int> indices(devices.size());

    int maxScore = 0;
    for (int i = 0; i < scores.size(); i++) {
      scores[i] = deviceRate(devices[i]);
      indices.push_back(i);
      maxScore = std::max(scores[i], maxScore);
    }

    std::sort(indices.begin(), indices.end(), [&](int lhs, int rhs) { return scores[lhs] < scores[rhs]; });

    LOG("[VK] Choosing device on score based criteria (%lu candidates)\n", scores.size());
    LOG("[VK] Device %d score %d (maxScore %d)\n", indices[0], scores[indices[0]], maxScore);
    deviceLog(devices[indices[0]]);

    return devices[indices[0]];
  }

  /* Queue famililies */

  struct QueueFamilyIndices {
    bool     hasGraphicsFamily = false;
    bool     hasComputeFamily  = false;
    bool     hasTransferFamily = false;
    bool     hasPresentFamily  = false;
    uint32_t graphicsFamily;
    uint32_t computeFamily;
    uint32_t transferFamily;
    uint32_t presentFamily;
  };

  QueueFamilyIndices physicalDeviceQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;
    uint32_t           queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
    int i = 0;
    for (const auto& queueFamily : queueFamilies) {

      bool graphics;
      bool compute;
      bool transfer;
      bool present;
      if (graphics = queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily    = i;
        indices.hasGraphicsFamily = true;
      }
      if (compute = queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) {
        indices.computeFamily    = i;
        indices.hasComputeFamily = true;
      }

      if (transfer = queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) {
        indices.transferFamily    = i;
        indices.hasTransferFamily = true;
      }
      if (mMainSurface->mSurface) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mMainSurface->mSurface, &presentSupport);
        if (presentSupport) {
          indices.hasPresentFamily = true;
          indices.presentFamily    = i;
        }

        present = presentSupport;
      } else {
        present = false;
      }

      if (isDebugBuild) {
        LOG("#%d0 \tGraphics %d \tPresent %d \tCompute %d \tTransfer %d\n", i, graphics, present, compute, transfer);
      }

      i++;
    }

    return indices;
  }

  bool physicalDeviceCheckExtensionSupport(VkPhysicalDevice device, std::vector<const char*>& extensions) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

    for (const auto& extension : availableExtensions) {
      requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
  }

  struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR        capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
  };

  SwapChainSupportDetails physicalDeviceSwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
      details.formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
      details.presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }
    return details;
  }

  struct SwapChain {
    VkSurfaceFormatKHR      surfaceFormat;
    VkPresentModeKHR        presentMode;
    VkExtent2D              extent;
    int                     imageCount;
    VkSwapchainKHR          swapChain;
    VkDevice                owner;
    std::vector<ptr<Image>> swapChainImages;

    ~SwapChain() {
      vkDestroySwapchainKHR(owner, swapChain, nullptr);
      LOG("[VK] Swapchain destroyed\n");
    }
  };


  //TODO: Choose method
  VkSurfaceFormatKHR swapChainChooseFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {

    for (const auto& availableFormat : availableFormats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
    }
    return availableFormats[0];
  }

  //TODO: Choose method
  VkPresentModeKHR swapChainChoosePresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    return VK_PRESENT_MODE_FIFO_KHR;
  }

  VkExtent2D swapChainChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, ISurface* surface) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
      return capabilities.currentExtent;
    } else {
      int width  = surface->getWidth();
      int height = surface->getHeight();


      VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)};

      actualExtent.width  = glm::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      actualExtent.height = glm::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

      return actualExtent;
    }
  }

  SwapChain* swapChainCreate(SwapChainSupportDetails swapChainSupport, ISurface* surfaceWindow, VkSurfaceKHR surface, VkDevice device) {
    SwapChain* swapChain = new SwapChain;
    swapChain->owner     = device;

    swapChain->surfaceFormat = swapChainChooseFormat(swapChainSupport.formats);
    swapChain->presentMode   = swapChainChoosePresentMode(swapChainSupport.presentModes);
    swapChain->extent        = swapChainChooseSwapExtent(swapChainSupport.capabilities, surfaceWindow);

    swapChain->imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 && swapChain->imageCount > swapChainSupport.capabilities.maxImageCount) {
      swapChain->imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;

    createInfo.minImageCount    = swapChain->imageCount;
    createInfo.imageFormat      = swapChain->surfaceFormat.format;
    createInfo.imageColorSpace  = swapChain->surfaceFormat.colorSpace;
    createInfo.imageExtent      = swapChain->extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    createInfo.presentMode  = swapChain->presentMode;
    createInfo.clipped      = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    HARD_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain->swapChain) == VK_SUCCESS, "Error creating swap chain\n");
    LOG("[VK] Swap chain created\n");

    std::vector<VkImage> images;
    uint32_t             imageCount;
    vkGetSwapchainImagesKHR(device, swapChain->swapChain, &imageCount, nullptr);
    swapChain->swapChainImages.resize(imageCount);
    images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain->swapChain, &imageCount, images.data());

    for (int i = 0; i < images.size(); i++) {
      swapChain->swapChainImages[i] = make_ptr<Image>(images[i], createInfo.imageFormat, device);
      swapChain->swapChainImages[i]->getDefaultView();
    }
    return swapChain;
  }

  struct DeviceDesc {
    int countGraphics = 1;
    int countCompute  = 1;
    int countTransfer = 1;

    std::vector<const char*> requestedValidationLayers;
  };


  struct Device {

    Device(DeviceDesc desc) { this->desc = desc; }

    ~Device() {
      if (swapChain)
        delete swapChain;
      vkDestroyDevice(device, nullptr);
      LOG("[VK] Device destroyed\n");
    }

    std::vector<const char*> deviceExtensions;

    std::vector<VkQueue> graphicQueues;
    std::vector<VkQueue> transferQueues;
    std::vector<VkQueue> computeQueues;
    VkQueue              presentQueue;

    SwapChainSupportDetails swapChainSupport;
    SwapChain*              swapChain = nullptr;

    VkDevice    device;
    DeviceDesc  desc;
    VKRenderer* renderer = nullptr;
  };

  Device* deviceCreate(VkPhysicalDevice physicalDevice, DeviceDesc desc) {

    Device* device   = new Device(desc);
    device->renderer = this;

    // QUEUES
    QueueFamilyIndices indices = physicalDeviceQueueFamilies(physicalDevice);

    VkDeviceQueueCreateInfo queueCreateInfos[3];

    int queueInfoBase = 0;
    int queueGraphics = -1;
    int queueCompute  = -1;
    int queueTransfer = -1;

    float queuePriority = 1.0f;

    if (indices.hasGraphicsFamily) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
      queueCreateInfo.queueCount       = desc.countGraphics;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      queueCreateInfos[queueInfoBase] = queueCreateInfo;
      queueGraphics                   = queueInfoBase++;

      LOG("[VK] Graphics found %d\n", queueGraphics);
      device->graphicQueues.resize(desc.countCompute);
    }

    if (indices.hasComputeFamily) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = indices.computeFamily;
      queueCreateInfo.queueCount       = desc.countCompute;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      queueCreateInfos[queueInfoBase] = queueCreateInfo;
      queueCompute                    = queueInfoBase++;
      LOG("[VK] Compute found %d\n", queueCompute);
      device->computeQueues.resize(desc.countCompute);
    }

    if (indices.hasTransferFamily) {
      VkDeviceQueueCreateInfo queueCreateInfo{};
      queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfo.queueFamilyIndex = indices.transferFamily;
      queueCreateInfo.queueCount       = desc.countTransfer;
      queueCreateInfo.pQueuePriorities = &queuePriority;

      queueCreateInfos[queueInfoBase] = queueCreateInfo;
      queueTransfer                   = queueInfoBase++;
      LOG("[VK] Transfer found %d\n", queueTransfer);
      device->transferQueues.resize(desc.countCompute);
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.pQueueCreateInfos    = queueCreateInfos;
    createInfo.queueCreateInfoCount = queueInfoBase;

    VERIFY(queueInfoBase > 0, "[VK] No queues supported on device\n");


    // FEATURES
    VkPhysicalDeviceFeatures deviceFeatures{};
    createInfo.pEnabledFeatures = &deviceFeatures;

    if (enableValidationLayers) {
      createInfo.enabledLayerCount   = desc.requestedValidationLayers.size();
      createInfo.ppEnabledLayerNames = desc.requestedValidationLayers.data();
    } else {
      createInfo.enabledLayerCount = 0;
    }

    if (this->_desc.surface->isOnline()) {
      LOG("[VK] Requesting swap chain extension support");
      device->deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    createInfo.enabledExtensionCount   = device->deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = device->deviceExtensions.data();

    VkDevice deviceHandle;

    if (isDebugBuild) {
      LOG("[VK] Requested extensions:\n");
      for (int i = 0; i < device->deviceExtensions.size(); i++) {
        LOG("[VK] -- %d %s\n", i, device->deviceExtensions[i]);
      }
    }

    HARD_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &deviceHandle) == VK_SUCCESS, "[VK] Error creating device");

    if (indices.hasGraphicsFamily) {
      for (int i = 0; i < desc.countGraphics; i++) {
        vkGetDeviceQueue(deviceHandle, indices.graphicsFamily, i, &device->graphicQueues[i]);
      }
    }

    if (indices.hasTransferFamily) {
      for (int i = 0; i < desc.countTransfer; i++) {
        vkGetDeviceQueue(deviceHandle, indices.transferFamily, i, &device->transferQueues[i]);
      }
    }

    if (indices.hasComputeFamily) {
      for (int i = 0; i < desc.countCompute; i++) {
        vkGetDeviceQueue(deviceHandle, indices.computeFamily, i, &device->computeQueues[i]);
      }
    }

    if (indices.hasPresentFamily) {
      vkGetDeviceQueue(deviceHandle, indices.presentFamily, 0, &device->presentQueue);
    }

    LOG("[VK] Device created\n");
    device->device = deviceHandle;

    if (mMainSurface) {
      device->swapChainSupport = physicalDeviceSwapChainSupport(physicalDevice, mMainSurface->mSurface);
      device->swapChain        = swapChainCreate(device->swapChainSupport, _desc.surface, mMainSurface->mSurface, device->device);
    }

    return device;
  }

  /* Surface */

  struct Surface {

    Surface(VKRenderer* renderer) { this->renderer = renderer; }

    ~Surface() {
      vkDestroySurfaceKHR(renderer->mInstance, mSurface, nullptr);
      LOG("[VK] Surface destroyed\n");
    }

    VKRenderer*  renderer;
    VkSurfaceKHR mSurface;
  };

  struct GLFWSurface : public Surface {

    GLFWSurface(VKRenderer* renderer) :
      Surface(renderer) {
      HARD_CHECK(glfwCreateWindowSurface(renderer->mInstance, (GLFWwindow*)renderer->desc().surface->native(), nullptr, &mSurface) == VK_SUCCESS, "Error creating surface\n");
      LOG("[VK] Surface created\n");
    }
  };

  Surface* surfaceCreate() {
    LOG("[VK] Creating surface\n");
    return new GLFWSurface(this);
  }

  VKRenderer(RendererDesc desc) {
    this->_desc = desc;
    instanceCreate();
    setupMessenger();
    DeviceDesc device_desc;
    device_desc.requestedValidationLayers = validationLayers;

    if (desc.surface->isOnline()) {
      mMainSurface = surfaceCreate();
    }
    mMainDevice = deviceCreate(pickPhysicalDevice(), device_desc);

    PipelineDesc pip_ci;
    pip_ci.shaders[FRAGMENT] = IO_readFile("assets/shaders/shader.frag.spv");
    pip_ci.shaders[VERTEX]   = IO_readFile("assets/shaders/shader.vert.spv");

    pip_ci.layout     = pipelineLayoutCreate(mMainDevice->device);
    pip_ci.renderPass = renderPassCreate(RenderPassDesc{}, mMainDevice->device);

    mPipeline = pipelineCreate(pip_ci, mMainDevice->device);
  }

  ~VKRenderer() {
    delete mMainDevice;
    delete mMainSurface;
    instanceDestroy();
  }

  virtual void render(Scene* scene) override {}
  virtual void upload(Scene* scene) override {}

  private:
  Surface*               mMainSurface = nullptr;
  Device*                mMainDevice  = nullptr;
  Pipeline*              mPipeline    = nullptr;
  VkInstance             mInstance;
  VkAllocationCallbacks* mAllocator = nullptr;
};

ENGINE_API IRenderer* rendererCreate(RendererDesc desc) { return new VKRenderer(desc); }

ENGINE_API RendererBackendDefaults rendererDefaults() {
  static RendererBackendDefaults def = {.glfw_noApi = true};
  return def;
}
} // namespace NextVideo
