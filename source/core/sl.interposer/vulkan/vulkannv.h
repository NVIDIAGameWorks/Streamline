// This file is for API functionality that isn't yet in a shipping SDK

#ifndef _VULKANNV_H
#define _VULKANNV_H 1

#define VK_NO_PROTOTYPES 1


#ifndef VK_NVX_CUDA

#define VK_STRUCTURE_TYPE_CU_MODULE_CREATE_INFO_NVX   ((VkStructureType)1000029000)
#define VK_STRUCTURE_TYPE_CU_FUNCTION_CREATE_INFO_NVX ((VkStructureType)1000029001)
#define VK_STRUCTURE_TYPE_CU_LAUNCH_INFO_NVX          ((VkStructureType)1000029002)
#define VK_OBJECT_TYPE_CU_MODULE_NVX                  ((VkObjectType)1000029000)
#define VK_OBJECT_TYPE_CU_FUNCTION_NVX                ((VkObjectType)1000029001)
#define VK_DEBUG_REPORT_OBJECT_TYPE_CU_MODULE_NVX_EXT   ((VkDebugReportObjectTypeEXT)1000029000)
#define VK_DEBUG_REPORT_OBJECT_TYPE_CU_FUNCTION_NVX_EXT ((VkDebugReportObjectTypeEXT)1000029001)


#define VK_NVX_CUDA 1
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkCuModuleNVX)
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkCuFunctionNVX)

#define VK_NVX_CUDA_SPEC_VERSION          1
#define VK_NVX_CUDA_EXTENSION_NAME        "VK_NVX_CUDA"

typedef struct VkCuModuleCreateInfoNVX {
  VkStructureType    sType;
  void*              pNext;
  size_t             dataSize;
  const void*        pData;
} VkCuModuleCreateInfoNVX;

typedef struct VkCuFunctionCreateInfoNVX {
  VkStructureType    sType;
  void*              pNext;
  VkCuModuleNVX      module;
  const char*        pName;
} VkCuFunctionCreateInfoNVX;

typedef struct VkCuLaunchInfoNVX {
  VkStructureType    sType;
  void*              pNext;
  VkCuFunctionNVX    function;
  uint32_t           gridDimX;
  uint32_t           gridDimY;
  uint32_t           gridDimZ;
  uint32_t           blockDimX;
  uint32_t           blockDimY;
  uint32_t           blockDimZ;
  uint32_t           sharedMemBytes;
  size_t             paramCount;
  const void**       pParams;
  size_t             extraCount;
  const void**       pExtras;
} VkCuLaunchInfoNVX;


typedef VkResult(VKAPI_PTR *PFN_vkCreateCuModuleNVX)(VkDevice device, const VkCuModuleCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuModuleNVX* pModule);
typedef VkResult(VKAPI_PTR *PFN_vkCreateCuFunctionNVX)(VkDevice device, const VkCuFunctionCreateInfoNVX* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCuFunctionNVX* pFunction);
typedef void (VKAPI_PTR *PFN_vkDestroyCuModuleNVX)(VkDevice device, VkCuModuleNVX module, const VkAllocationCallbacks* pAllocator);
typedef void (VKAPI_PTR *PFN_vkDestroyCuFunctionNVX)(VkDevice device, VkCuFunctionNVX function, const VkAllocationCallbacks* pAllocator);
typedef void (VKAPI_PTR *PFN_vkCmdCuLaunchKernelNVX)(VkCommandBuffer commandBuffer, const VkCuLaunchInfoNVX* pLaunchInfo);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateCuModuleNVX(
  VkDevice                                    device,
  const VkCuModuleCreateInfoNVX*              pCreateInfo,
  const VkAllocationCallbacks*                pAllocator,
  VkCuModuleNVX*                              pModule);

VKAPI_ATTR VkResult VKAPI_CALL vkCreateCuFunctionNVX(
  VkDevice                                    device,
  const VkCuFunctionCreateInfoNVX*            pCreateInfo,
  const VkAllocationCallbacks*                pAllocator,
  VkCuFunctionNVX*                            pFunction);

VKAPI_ATTR void VKAPI_CALL vkDestroyCuModuleNVX(
  VkDevice                                    device,
  VkCuModuleNVX                               module,
  const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR void VKAPI_CALL vkDestroyCuFunctionNVX(
  VkDevice                                    device,
  VkCuFunctionNVX                             function,
  const VkAllocationCallbacks*                pAllocator);

VKAPI_ATTR void VKAPI_CALL vkCmdCuLaunchKernelNVX(
  VkCommandBuffer                             commandBuffer,
  const VkCuLaunchInfoNVX*                    pLaunchInfo);
#endif

#endif //ndef VK_NVX_CUDA

#ifndef VK_NVX_image_view_handle

#define VK_STRUCTURE_TYPE_IMAGE_VIEW_HANDLE_INFO_NVX  ((VkStructureType)1000030000)

#define VK_NVX_image_view_handle 1
#define VK_NVX_IMAGE_VIEW_HANDLE_SPEC_VERSION 1
#define VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME "VK_NVX_image_view_handle"

typedef struct VkImageViewHandleInfoNVX {
  VkStructureType     sType;
  void*               pNext;
  VkImageView         imageView;
  VkDescriptorType    descriptorType;
  VkSampler           sampler;
} VkImageViewHandleInfoNVX;


typedef uint32_t(VKAPI_PTR *PFN_vkGetImageViewHandleNVX)(VkDevice device, const VkImageViewHandleInfoNVX* pInfo);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR uint32_t VKAPI_CALL vkGetImageViewHandleNVX(
  VkDevice                                    device,
  const VkImageViewHandleInfoNVX*             pInfo);
#endif

#endif  //ndef VK_NVX_image_view_handle

#ifndef VK_EXT_buffer_device_address

#define VK_EXT_buffer_device_address 1
typedef uint64_t VkDeviceAddress;

#define VK_EXT_BUFFER_DEVICE_ADDRESS_SPEC_VERSION 1
#define VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME "VK_EXT_buffer_device_address"

#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_ADDRESS_FEATURES_EXT ((VkStructureType)1000244000)
#define VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_EXT ((VkStructureType)1000244001)
#define VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_CREATE_INFO_EXT ((VkStructureType)1000244002)
#define VK_ERROR_INVALID_DEVICE_ADDRESS_EXT ((VkResult)1000244000)

#define VK_BUFFER_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_EXT 0x00000010
#define VK_BUFFER_USAGE_SHADER_ADDRESS_BIT_EXT 0x00002000

typedef struct VkPhysicalDeviceBufferAddressFeaturesEXT {
  VkStructureType    sType;
  void*              pNext;
  VkBool32           bufferDeviceAddress;
  VkBool32           bufferDeviceAddressCaptureReplay;
} VkPhysicalDeviceBufferAddressFeaturesEXT;

typedef struct VkBufferDeviceAddressInfoEXT {
  VkStructureType    sType;
  const void*        pNext;
  VkBuffer           buffer;
} VkBufferDeviceAddressInfoEXT;

typedef struct VkBufferDeviceAddressCreateInfoEXT {
  VkStructureType    sType;
  const void*        pNext;
  VkDeviceSize       deviceAddress;
} VkBufferDeviceAddressCreateInfoEXT;


typedef VkDeviceAddress(VKAPI_PTR *PFN_vkGetBufferDeviceAddressEXT)(VkDevice device, const VkBufferDeviceAddressInfoEXT* pInfo);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkDeviceAddress VKAPI_CALL vkGetBufferDeviceAddressEXT(
  VkDevice                                    device,
  const VkBufferDeviceAddressInfoEXT*         pInfo);
#endif

#endif //ndef VK_EXT_buffer_device_address

#endif  //ndef _VULKANNV_H
