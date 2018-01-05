#version 120

varying vec2 diffuseMapUV;

uniform int diffuseMapUVIndex;

uniform sampler2D diffuseMap;

varying float alphaPassthrough;

void main()
{
	float diffuseMapAlpha = 1.0;

	if (diffuseMapUVIndex < 0)
		diffuseMapAlpha = texture2D(diffuseMap, diffuseMapUV).a;

	gl_FragData[0] = vec4(0.0, 0.0, 0.0, alphaPassthrough * diffuseMapAlpha);
	if (gl_FragData[0].a <= 0.0)
		discard;
}
