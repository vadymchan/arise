#include "gfx/rhi/backends/dx12/dxc_util.h"

#include <d3d12shader.h>

#include <algorithm>
#include <cctype>
#include <unordered_set>

#ifdef ARISE_USE_SPIRV_REFLECT
#include <spirv_reflect.h>
#endif

namespace arise {

std::shared_ptr<IDxcBlob> DxcUtil::compileHlslCode(const std::string&          hlslCode,
                                                   gfx::rhi::ShaderStageFlag   stage,
                                                   const std::wstring&         entryPoint,
                                                   ShaderBackend               backend,
                                                   const OptionalShaderParams& optionalParams) {
  if (!m_dxcCreateFn) {
    GlobalLogger::Log(LogLevel::Error, "DXC library not loaded properly.");
    return nullptr;
  }

  std::wstring targetProfile = getTargetProfile_(stage);
  if (targetProfile.empty()) {
    GlobalLogger::Log(LogLevel::Error, "Invalid shader stage provided.");
    return nullptr;
  }

  GlobalLogger::Log(LogLevel::Info, "Compiling shader for target: " + wstring_to_utf8_(targetProfile));

  IDxcCompiler3* compilerRaw = nullptr;
  IDxcUtils*     utilsRaw    = nullptr;
  if (FAILED(m_dxcCreateFn(CLSID_DxcCompiler, IID_PPV_ARGS(&compilerRaw)))
      || FAILED(m_dxcCreateFn(CLSID_DxcUtils, IID_PPV_ARGS(&utilsRaw)))) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create DXC instances.");
    return nullptr;
  }

  auto compiler = MakeDxcSharedPtr(compilerRaw);
  auto utils    = MakeDxcSharedPtr(utilsRaw);

  IDxcIncludeHandler* includeHandlerRaw = nullptr;
  if (FAILED(utils->CreateDefaultIncludeHandler(&includeHandlerRaw))) {
    return nullptr;
  }
  auto includeHandler = MakeDxcSharedPtr(includeHandlerRaw);

  DxcBuffer sourceBuf;
  sourceBuf.Ptr      = hlslCode.data();
  sourceBuf.Size     = hlslCode.size();
  sourceBuf.Encoding = DXC_CP_UTF8;

  std::vector<std::wstring> argStrings;
  std::vector<LPCWSTR>      args;

  argStrings.push_back(L"-E");
  argStrings.push_back(entryPoint);
  argStrings.push_back(L"-T");
  argStrings.push_back(targetProfile);

#ifdef _DEBUG
  argStrings.push_back(L"-Zi");
  argStrings.push_back(L"-Qembed_debug");
  argStrings.push_back(L"-Od");
#else
  argStrings.push_back(L"-O3");
#endif

  if (backend == ShaderBackend::SPIRV) {
    argStrings.push_back(L"-spirv");
  }

  // includes
  for (auto& inc : optionalParams.includeDirs) {
    argStrings.push_back(L"-I");
    argStrings.push_back(inc);
  }
  // defines
  for (auto& def : optionalParams.preprocessorDefs) {
    argStrings.push_back(L"-D");
    argStrings.push_back(def);
  }
  // extra
  for (auto& ex : optionalParams.extraArgs) {
    argStrings.push_back(ex);
  }

  // gather pointers
  args.reserve(argStrings.size());
  for (auto& s : argStrings) {
    args.push_back(s.c_str());
  }

  IDxcResult* resultRaw = nullptr;
  HRESULT     hr
      = compiler->Compile(&sourceBuf, args.data(), (UINT)args.size(), includeHandler.get(), IID_PPV_ARGS(&resultRaw));
  if (FAILED(hr) || !resultRaw) {
    GlobalLogger::Log(LogLevel::Error, "Failed to compile shader.");
    return nullptr;
  }
  auto result = MakeDxcSharedPtr(resultRaw);

  IDxcBlobUtf8* errorsRaw = nullptr;
  hr                      = result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errorsRaw), nullptr);
  auto errorsPtr          = MakeDxcSharedPtr(errorsRaw);
  if (errorsPtr && errorsPtr->GetStringLength() > 0) {
    GlobalLogger::Log(
        LogLevel::Warning,
        std::string(errorsPtr->GetStringPointer(), errorsPtr->GetStringPointer() + errorsPtr->GetStringLength()));
  }

  HRESULT status;
  result->GetStatus(&status);
  if (FAILED(status)) {
    GlobalLogger::Log(LogLevel::Error, "Shader compilation failed.");
    return nullptr;
  }

  IDxcBlob* blobRaw = nullptr;
  hr                = result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blobRaw), nullptr);
  if (FAILED(hr) || !blobRaw) {
    GlobalLogger::Log(LogLevel::Error, "Failed to retrieve shader blob.");
    return nullptr;
  }

  return MakeDxcSharedPtr(blobRaw);
}

gfx::rhi::ShaderMeta DxcUtil::reflectShader(const std::shared_ptr<IDxcBlob>& shaderBlob,
                                            gfx::rhi::ShaderStageFlag        stage,
                                            ShaderBackend                    backend) {
  if (!shaderBlob) {
    GlobalLogger::Log(LogLevel::Error, "Invalid shader blob provided for reflection.");
    return {};
  }

  switch (backend) {
    case ShaderBackend::DXIL:
      return reflectDxil_(shaderBlob, stage);
    case ShaderBackend::SPIRV:
      return reflectSpirv_(shaderBlob, stage);
    default:
      GlobalLogger::Log(LogLevel::Error, "Unsupported shader backend for reflection.");
      return {};
  }
}

bool DxcUtil::initialize() {
  if (m_libHandle) {
    return true;
  }

#if defined(_WIN32)
  m_libHandle = LoadLibraryW(DXC_COMPILER_LIBRARY);
#else
  m_libHandle = dlopen(DXC_COMPILER_LIBRARY, RTLD_LAZY);
#endif

  if (!m_libHandle) {
    return false;
  }

  m_dxcCreateFn = reinterpret_cast<DxcCreateInstanceProc>(GET_DXC_SYMBOL(m_libHandle, "DxcCreateInstance"));

  if (!m_dxcCreateFn) {
#if defined(_WIN32)
    FreeLibrary(m_libHandle);
#else
    dlclose(m_libHandle);
#endif
    m_libHandle = nullptr;
    return false;
  }

  return true;
}

std::wstring DxcUtil::getTargetProfile_(gfx::rhi::ShaderStageFlag stage) {
  static const std::wstring suffix = L"_6_7";
  switch (stage) {
    case gfx::rhi::ShaderStageFlag::Vertex:
      return L"vs" + suffix;
    case gfx::rhi::ShaderStageFlag::Fragment:
      return L"ps" + suffix;
    case gfx::rhi::ShaderStageFlag::Compute:
      return L"cs" + suffix;
    case gfx::rhi::ShaderStageFlag::Geometry:
      return L"gs" + suffix;
    case gfx::rhi::ShaderStageFlag::TessellationControl:
      return L"hs" + suffix;
    case gfx::rhi::ShaderStageFlag::TessellationEvaluation:
      return L"ds" + suffix;

    case gfx::rhi::ShaderStageFlag::Raytracing:
    case gfx::rhi::ShaderStageFlag::RaytracingRaygen:
    case gfx::rhi::ShaderStageFlag::RaytracingMiss:
    case gfx::rhi::ShaderStageFlag::RaytracingClosesthit:
    case gfx::rhi::ShaderStageFlag::RaytracingAnyhit:
      return L"lib" + suffix;
    default:
      return L"";  // unsupported combination
  }
}

std::string DxcUtil::readFile_(const std::filesystem::path& path) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) {
    GlobalLogger::Log(LogLevel::Error, "Failed to open shader file: " + path.string());
    return {};
  }
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

std::string DxcUtil::wstring_to_utf8_(const std::wstring& wstr) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
  return conv.to_bytes(wstr);
}

gfx::rhi::ShaderMeta DxcUtil::reflectDxil_(const std::shared_ptr<IDxcBlob>& shaderBlob,
                                           gfx::rhi::ShaderStageFlag        stage) {
  gfx::rhi::ShaderMeta meta;

  if (!m_dxcCreateFn) {
    GlobalLogger::Log(LogLevel::Error, "DXC library not loaded for reflection.");
    return meta;
  }

  IDxcUtils* utilsRaw = nullptr;
  if (FAILED(m_dxcCreateFn(CLSID_DxcUtils, IID_PPV_ARGS(&utilsRaw)))) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create DXC Utils for reflection.");
    return meta;
  }
  auto utils = MakeDxcSharedPtr(utilsRaw);

  DxcBuffer reflectionBuffer;
  reflectionBuffer.Ptr      = shaderBlob->GetBufferPointer();
  reflectionBuffer.Size     = shaderBlob->GetBufferSize();
  reflectionBuffer.Encoding = 0;

  ID3D12ShaderReflection* reflectionRaw = nullptr;
  HRESULT                 hr            = utils->CreateReflection(&reflectionBuffer, IID_PPV_ARGS(&reflectionRaw));
  if (FAILED(hr) || !reflectionRaw) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create shader reflection for DXIL.");
    return meta;
  }
  auto reflection = MakeDxcSharedPtr(reflectionRaw);

  D3D12_SHADER_DESC shaderDesc;
  if (FAILED(reflection->GetDesc(&shaderDesc))) {
    GlobalLogger::Log(LogLevel::Error, "Failed to get shader description.");
    return meta;
  }

  // Extract resource bindings
  for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
    D3D12_SHADER_INPUT_BIND_DESC bindDesc;
    if (FAILED(reflection->GetResourceBindingDesc(i, &bindDesc))) {
      continue;
    }

    gfx::rhi::ShaderResourceBinding binding;
    binding.name            = bindDesc.Name;
    binding.binding         = bindDesc.BindPoint;
    binding.set             = bindDesc.Space;
    binding.descriptorCount = bindDesc.BindCount == 0 ? UINT32_MAX : bindDesc.BindCount;  // 0 means unbounded
    binding.type            = convertDxilResourceType_(bindDesc.Type);
    binding.stageFlags      = stage;
    binding.nonUniform      = (bindDesc.BindCount == 0);  // unbounded arrays are typically non-uniform

    meta.bindings.push_back(binding);
  }

  // Extract vertex inputs (only for vertex shaders)
  if (stage == gfx::rhi::ShaderStageFlag::Vertex) {
    for (UINT i = 0; i < shaderDesc.InputParameters; ++i) {
      D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
      if (FAILED(reflection->GetInputParameterDesc(i, &paramDesc))) {
        continue;
      }

      gfx::rhi::ShaderVertexInput vertexInput;
      vertexInput.semanticName = paramDesc.SemanticName ? paramDesc.SemanticName : "";
      vertexInput.location     = paramDesc.SemanticIndex;

      // Determine format from component mask and type
      uint32_t components = 0;
      if (paramDesc.Mask & 0x1) {
        components++;
      }
      if (paramDesc.Mask & 0x2) {
        components++;
      }
      if (paramDesc.Mask & 0x4) {
        components++;
      }
      if (paramDesc.Mask & 0x8) {
        components++;
      }
      components = components > 0 ? components : 1;

      // Convert component type to TextureFormat
      switch (paramDesc.ComponentType) {
        case D3D_REGISTER_COMPONENT_FLOAT32:
          if (components == 1) {
            vertexInput.format = gfx::rhi::TextureFormat::R32f;
          } else if (components == 2) {
            vertexInput.format = gfx::rhi::TextureFormat::Rg32f;
          } else if (components == 3) {
            vertexInput.format = gfx::rhi::TextureFormat::Rgb32f;
          } else {
            vertexInput.format = gfx::rhi::TextureFormat::Rgba32f;
          }
          break;
          // TODO: add other types (int, uint) apart from float
        default:
          vertexInput.format = gfx::rhi::TextureFormat::Rgba32f;
          break;
      }

      static const std::unordered_set<std::string> builtInSemantics = {"SV_VertexID", "SV_InstanceID", "SV_Position"};

      if (builtInSemantics.contains(vertexInput.semanticName)) {
        continue;  // Skip built-ins, don't add to vertexInputs
      }

      meta.vertexInputs.push_back(vertexInput);
    }
  }

  // Extract push constants
  for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i) {
    ID3D12ShaderReflectionConstantBuffer* cbuffer = reflection->GetConstantBufferByIndex(i);
    if (!cbuffer) {
      continue;
    }

    D3D12_SHADER_BUFFER_DESC bufferDesc;
    if (SUCCEEDED(cbuffer->GetDesc(&bufferDesc))) {
      // Simple heuristic: if CB is named like push constants and size is small, treat as push constant
      std::string cbName      = bufferDesc.Name ? bufferDesc.Name : "";
      std::string lowerCbName = cbName;
      std::transform(lowerCbName.begin(), lowerCbName.end(), lowerCbName.begin(), ::tolower);

      if (lowerCbName.find("push") != std::string::npos) {
        meta.pushConstantSize = std::max(meta.pushConstantSize, bufferDesc.Size);
      }
    }
  }

  GlobalLogger::Log(LogLevel::Info,
                    "Extracted " + std::to_string(meta.bindings.size()) + " resource bindings from DXIL shader.");

  return meta;
}

gfx::rhi::ShaderMeta DxcUtil::reflectSpirv_(const std::shared_ptr<IDxcBlob>& shaderBlob,
                                            gfx::rhi::ShaderStageFlag        stage) {
  gfx::rhi::ShaderMeta meta;

#ifdef ARISE_USE_SPIRV_REFLECT
  SpvReflectShaderModule module;
  SpvReflectResult       result
      = spvReflectCreateShaderModule(shaderBlob->GetBufferSize(), shaderBlob->GetBufferPointer(), &module);

  if (result != SPV_REFLECT_RESULT_SUCCESS) {
    GlobalLogger::Log(LogLevel::Error, "Failed to create SPIRV reflection module.");
    return meta;
  }

  // Extract descriptor bindings
  uint32_t bindingCount = 0;
  result                = spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr);
  if (result == SPV_REFLECT_RESULT_SUCCESS && bindingCount > 0) {
    std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
    result = spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data());

    if (result == SPV_REFLECT_RESULT_SUCCESS) {
      for (const auto* binding : bindings) {
        gfx::rhi::ShaderResourceBinding rhiBinding;
        rhiBinding.name            = binding->name ? binding->name : "";
        rhiBinding.binding         = binding->binding;
        rhiBinding.set             = binding->set;
        rhiBinding.descriptorCount = binding->count;
        rhiBinding.stageFlags      = stage;
        rhiBinding.nonUniform      = (binding->count == 0);  // unbounded arrays are typically non-uniform

        // Convert SPIRV descriptor type to RHI type
        switch (binding->descriptor_type) {
          case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
            rhiBinding.type = gfx::rhi::ShaderBindingType::Sampler;
            break;
          case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            rhiBinding.type = gfx::rhi::ShaderBindingType::TextureSamplerSrv;
            break;
          case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            rhiBinding.type = gfx::rhi::ShaderBindingType::TextureSrv;
            break;
          case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            rhiBinding.type = gfx::rhi::ShaderBindingType::TextureUav;
            break;
          case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            rhiBinding.type = gfx::rhi::ShaderBindingType::Uniformbuffer;
            break;
          case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            rhiBinding.type = gfx::rhi::ShaderBindingType::BufferSrv;
            break;
          default:
            rhiBinding.type = gfx::rhi::ShaderBindingType::Uniformbuffer;
            break;
        }

        meta.bindings.push_back(rhiBinding);
      }
    }
  }

  // Extract vertex inputs (only for vertex shaders)
  if (stage == gfx::rhi::ShaderStageFlag::Vertex) {
    uint32_t inputVarCount = 0;
    result                 = spvReflectEnumerateInputVariables(&module, &inputVarCount, nullptr);
    if (result == SPV_REFLECT_RESULT_SUCCESS && inputVarCount > 0) {
      std::vector<SpvReflectInterfaceVariable*> inputVars(inputVarCount);
      result = spvReflectEnumerateInputVariables(&module, &inputVarCount, inputVars.data());

      if (result == SPV_REFLECT_RESULT_SUCCESS) {
        for (const auto* inputVar : inputVars) {
          if (!inputVar || inputVar->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN) {
            continue;  // Skip built-ins
          }

          gfx::rhi::ShaderVertexInput vertexInput;
          vertexInput.location = inputVar->location;

          // TODO: placeholder for semantic name mapping. consider using a more robust mapping
          if (vertexInput.location == 0) {
            vertexInput.semanticName = "POSITION";
          } else if (vertexInput.location == 1) {
            vertexInput.semanticName = "TEXCOORD";
          } else if (vertexInput.location == 2) {
            vertexInput.semanticName = "NORMAL";
          } else if (vertexInput.location == 3) {
            vertexInput.semanticName = "TANGENT";
          } else if (vertexInput.location == 4) {
            vertexInput.semanticName = "BITANGENT";
          } else if (vertexInput.location == 5) {
            vertexInput.semanticName = "COLOR";
          } else if (vertexInput.location == 6) {
            vertexInput.semanticName = "INSTANCE";
          } else {
            vertexInput.semanticName = "TEXCOORD";
          }

          // format information
          if (inputVar->type_description) {
            const SpvReflectTypeDescription* typeDesc = inputVar->type_description;

            uint32_t componentCount = typeDesc->traits.numeric.vector.component_count;
            if (componentCount == 0) {
              componentCount = 1;  // scalar
            }

            if (typeDesc->traits.array.dims_count > 0) {
              vertexInput.arraySize = typeDesc->traits.array.dims[0];
            }

            switch (typeDesc->traits.numeric.scalar.signedness) {
              // TODO: add support for signed/unsigned integers (currently assume all inputs are float)
              default:  // float
                if (componentCount == 1) {
                  vertexInput.format = gfx::rhi::TextureFormat::R32f;
                } else if (componentCount == 2) {
                  vertexInput.format = gfx::rhi::TextureFormat::Rg32f;
                } else if (componentCount == 3) {
                  vertexInput.format = gfx::rhi::TextureFormat::Rgb32f;
                } else {
                  vertexInput.format = gfx::rhi::TextureFormat::Rgba32f;
                }
                break;
            }
          }

          // TODO: dirty solution - should be done in a more robust way
          if (vertexInput.semanticName == "INSTANCE") {
            for (uint32_t row = 0; row < 4; ++row) {
              gfx::rhi::ShaderVertexInput matrixRowInput = vertexInput;
              matrixRowInput.location                    = vertexInput.location + row;
              matrixRowInput.arraySize                   = 1;
              matrixRowInput.format                      = gfx::rhi::TextureFormat::Rgba32f;

              meta.vertexInputs.push_back(matrixRowInput);
            }
          } else {
            meta.vertexInputs.push_back(vertexInput);
          }
        }
      }
    }
  }

  // push constants
  uint32_t pushConstantCount = 0;
  result                     = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, nullptr);
  if (result == SPV_REFLECT_RESULT_SUCCESS && pushConstantCount > 0) {
    std::vector<SpvReflectBlockVariable*> pushConstants(pushConstantCount);
    result = spvReflectEnumeratePushConstantBlocks(&module, &pushConstantCount, pushConstants.data());

    if (result == SPV_REFLECT_RESULT_SUCCESS) {
      for (const auto* pushConstant : pushConstants) {
        meta.pushConstantSize = std::max(meta.pushConstantSize, pushConstant->size);
      }
    }
  }

  spvReflectDestroyShaderModule(&module);

  GlobalLogger::Log(LogLevel::Info,
                    "Extracted " + std::to_string(meta.bindings.size()) + " resource bindings from SPIRV shader.");

#else
  GlobalLogger::Log(LogLevel::Warning, "SPIRV reflection not available - SPIRV-Reflect not enabled.");
#endif

  return meta;
}

gfx::rhi::ShaderBindingType DxcUtil::convertDxilResourceType_(D3D_SHADER_INPUT_TYPE type) {
  switch (type) {
    case D3D_SIT_CBUFFER:
      return gfx::rhi::ShaderBindingType::Uniformbuffer;
    case D3D_SIT_TEXTURE:
      return gfx::rhi::ShaderBindingType::TextureSrv;
    case D3D_SIT_SAMPLER:
      return gfx::rhi::ShaderBindingType::Sampler;
    case D3D_SIT_UAV_RWTYPED:
    case D3D_SIT_UAV_RWSTRUCTURED:
    case D3D_SIT_UAV_RWBYTEADDRESS:
    case D3D_SIT_UAV_APPEND_STRUCTURED:
    case D3D_SIT_UAV_CONSUME_STRUCTURED:
    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
      return gfx::rhi::ShaderBindingType::TextureUav;
    case D3D_SIT_STRUCTURED:
    case D3D_SIT_BYTEADDRESS:
      return gfx::rhi::ShaderBindingType::BufferSrv;
    default:
      return gfx::rhi::ShaderBindingType::Uniformbuffer;
  }
}

}  // namespace arise