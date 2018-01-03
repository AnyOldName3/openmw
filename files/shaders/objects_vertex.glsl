#version 120

#define SHADOWS @shadows_enabled

#if @diffuseMap
varying vec2 diffuseMapUV;
#endif

#if @darkMap
varying vec2 darkMapUV;
#endif

#if @detailMap
varying vec2 detailMapUV;
#endif

#if @decalMap
varying vec2 decalMapUV;
#endif

#if @emissiveMap
varying vec2 emissiveMapUV;
#endif

#if @normalMap
varying vec2 normalMapUV;
varying vec4 passTangent;
#endif

#if @envMap
varying vec2 envMapUV;
#endif

#if @specularMap
varying vec2 specularMapUV;
#endif

varying float depth;

#define PER_PIXEL_LIGHTING (@normalMap || @forcePPL)

#if !PER_PIXEL_LIGHTING
varying vec4 lighting;
varying vec3 shadowDiffuseLighting;
#else
varying vec4 passColor;
#endif
varying vec3 passViewPos;
varying vec3 passNormal;

#if SHADOWS
	@foreach shadow_texture_unit_index @shadow_texture_unit_list
		uniform int shadowTextureUnit@shadow_texture_unit_index;
		varying vec4 shadowSpaceCoords@shadow_texture_unit_index;
	@endforeach

    uniform bool shadowMapMode;
#if !PER_PIXEL_LIGHTING
    varying float passAlpha;
#endif //PPL
#endif // SHADOWS

#include "lighting.glsl"

void main(void)
{
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
    depth = gl_Position.z;

    vec4 viewPos = (gl_ModelViewMatrix * gl_Vertex);
    gl_ClipVertex = viewPos;
    vec3 viewNormal = normalize((gl_NormalMatrix * gl_Normal).xyz);

#if @envMap
    vec3 viewVec = normalize(viewPos.xyz);
    vec3 r = reflect( viewVec, viewNormal );
    float m = 2.0 * sqrt( r.x*r.x + r.y*r.y + (r.z+1.0)*(r.z+1.0) );
    envMapUV = vec2(r.x/m + 0.5, r.y/m + 0.5);
#endif

#if @diffuseMap
    diffuseMapUV = (gl_TextureMatrix[@diffuseMapUV] * gl_MultiTexCoord@diffuseMapUV).xy;
#endif

#if @darkMap
    darkMapUV = (gl_TextureMatrix[@darkMapUV] * gl_MultiTexCoord@darkMapUV).xy;
#endif

#if @detailMap
    detailMapUV = (gl_TextureMatrix[@detailMapUV] * gl_MultiTexCoord@detailMapUV).xy;
#endif

#if @decalMap
    decalMapUV = (gl_TextureMatrix[@decalMapUV] * gl_MultiTexCoord@decalMapUV).xy;
#endif

#if @emissiveMap
    emissiveMapUV = (gl_TextureMatrix[@emissiveMapUV] * gl_MultiTexCoord@emissiveMapUV).xy;
#endif

#if @normalMap
    normalMapUV = (gl_TextureMatrix[@normalMapUV] * gl_MultiTexCoord@normalMapUV).xy;
    passTangent = gl_MultiTexCoord7.xyzw;
#endif

#if @specularMap
    specularMapUV = (gl_TextureMatrix[@specularMapUV] * gl_MultiTexCoord@specularMapUV).xy;
#endif

#if SHADOWS
    if (shadowMapMode)
    {
        // We only care about the depth buffer so we skip the 'colouring in', which should be faster on many GPUs.
#if !PER_PIXEL_LIGHTING
        passAlpha = getAlpha(gl_Color);
#endif
        passColor = gl_Color;
		return;
    }
#endif // SHADOWS

#if !PER_PIXEL_LIGHTING
    lighting = doLighting(viewPos.xyz, viewNormal, gl_Color, shadowDiffuseLighting);
#else
    passColor = gl_Color;
#endif
    passViewPos = viewPos.xyz;
    passNormal = gl_Normal.xyz;

#if SHADOWS
	// This matrix has the opposite handedness to the others used here, so multiplication must have the vector to the left. Alternatively it could be transposed after construction, but that's extra work for the GPU just to make the code look a tiny bit cleaner.
	mat4 eyePlaneMat;
	@foreach shadow_texture_unit_index @shadow_texture_unit_list
		eyePlaneMat = mat4(gl_EyePlaneS[shadowTextureUnit@shadow_texture_unit_index], gl_EyePlaneT[shadowTextureUnit@shadow_texture_unit_index], gl_EyePlaneR[shadowTextureUnit@shadow_texture_unit_index], gl_EyePlaneQ[shadowTextureUnit@shadow_texture_unit_index]);
		shadowSpaceCoords@shadow_texture_unit_index = viewPos * eyePlaneMat;
	@endforeach
#endif // SHADOWS
}
