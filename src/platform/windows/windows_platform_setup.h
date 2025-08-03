
#ifndef ARISE_WINDOWS_PLATFORM_SETUP_H
#define ARISE_WINDOWS_PLATFORM_SETUP_H

#if defined(_WIN32) || defined(_WIN64)
/**
 * @def ARISE_PLATFORM_WINDOWS
 * @brief Indicates that the target platform is Windows (either 32-bit or
 * 64-bit)
 *
 * Defined when compiling on a Windows platform, enabling Windows-specific
 * code and dependencies within the engine
 */
#define ARISE_PLATFORM_WINDOWS

// Disable min and max macros from Windows headers
#define NOMINMAX

// TODO: think about what include to use and what version (e.g. dxgi1_6)
#include <d3d12.h>
#include <directx/d3dx12.h>
#include <dxcapi.h>   // for shader compilation
#include <dxgi1_6.h>
#include <windows.h>  // Win API
#include <wrl.h>      // manages the lifetime of COM objects and interfaces

using Microsoft::WRL::ComPtr;

#endif

#endif  // ARISE_WINDOWS_PLATFORM_SETUP_H
