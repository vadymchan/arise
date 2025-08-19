#ifndef SHADER_SEMANTICS_HLSLI
#define SHADER_SEMANTICS_HLSLI

// Vertex attribute location mappings
// These locations are fixed and must match the C++ side semantic->location table
#define _POSITION_LOC        0
#define _TEXCOORD_LOC        1  
#define _NORMAL_LOC          2
#define _TANGENT_LOC         3
#define _BITANGENT_LOC       4
#define _COLOR_LOC           5
#define _INSTANCE_LOC        6
// Note: INSTANCE matrix occupies 4 consecutive locations starting at 6: {6,7,8,9}
//       so custom debug attributes must start at 10 to avoid overlap
#define _LOCAL_MIN_CORNER_LOC    10
#define _LOCAL_MAX_CORNER_LOC    11
#define _WORLD_MIN_CORNER_LOC    12
#define _WORLD_MAX_CORNER_LOC    13

// Helper macro to concatenate semantic name with location number
#define CONCAT(a, b) a##b
#define SEMANTIC_WITH_LOC(semantic, loc) CONCAT(semantic, loc)

// Vertex attribute macro for unified DirectX/Vulkan vertex input declarations  
#ifdef __spirv__
  #define VERTEX_ATTR(semantic, type, name) [[vk::location(_##semantic##_LOC)]] type name : SEMANTIC_WITH_LOC(semantic, _##semantic##_LOC)
#else  
  #define VERTEX_ATTR(semantic, type, name) type name : SEMANTIC_WITH_LOC(semantic, _##semantic##_LOC)
#endif

#endif // SHADER_SEMANTICS_HLSLI