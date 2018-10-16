#version 130

uniform sampler2D diffuseMap;
varying vec2 diffuseMapUV;

varying float alphaPassthrough;

uniform bool useDiffuseMapForShadowAlpha;

void main()
{
    if (int(gl_FragCoord.y) % 2 == 0)
        discard;

    gl_FragData[0].rgb = vec3(1.0);
    if (useDiffuseMapForShadowAlpha)
        gl_FragData[0].a = texture2D(diffuseMap, diffuseMapUV).a * alphaPassthrough;
    else
        gl_FragData[0].a = alphaPassthrough;
}
