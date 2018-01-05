#version 120

varying vec2 diffuseMapUV;

uniform int diffuseMapUVIndex;

varying float alphaPassthrough;

void main(void)
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;

	switch (diffuseMapUVIndex)
	{
		case 0:
			diffuseMapUV = (gl_TextureMatrix[0] * gl_MultiTexCoord0).xy;
			break;
		case 1:
			diffuseMapUV = (gl_TextureMatrix[1] * gl_MultiTexCoord1).xy;
			break;
		case 2:
			diffuseMapUV = (gl_TextureMatrix[2] * gl_MultiTexCoord2).xy;
			break;
		case 3:
			diffuseMapUV = (gl_TextureMatrix[3] * gl_MultiTexCoord3).xy;
			break;
		case 4:
			diffuseMapUV = (gl_TextureMatrix[4] * gl_MultiTexCoord4).xy;
			break;
		case 5:
			diffuseMapUV = (gl_TextureMatrix[5] * gl_MultiTexCoord5).xy;
			break;
		case 6:
			diffuseMapUV = (gl_TextureMatrix[6] * gl_MultiTexCoord6).xy;
			break;
		case 7:
			diffuseMapUV = (gl_TextureMatrix[7] * gl_MultiTexCoord7).xy;
			break;
		default: // No diffuse map
			break;
	}

	if (colorMode == 2)
    {
        alphaPassthrough = gl_Color.a;
    }
    else
    {
        alphaPassthrough = gl_FrontMaterial.diffuse.a;
    }
}
