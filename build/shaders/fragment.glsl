#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;
in vec4 vLightSpacePos;

uniform vec3 uLightDir;
uniform vec3 uBaseColor;
uniform vec3 uCameraPos;
uniform float uSpecularStrength;
uniform float uShininess;
uniform sampler2D uShadowMap;
uniform vec3 uPointLightPos;
uniform vec3 uPointLightColor;
uniform float uPointLightIntensity;
uniform float uAmbientStrength;

out vec4 fragColor;

float computeShadow(vec4 lightSpacePos, vec3 normal, vec3 lightDir) {
    vec3 projCoords = lightSpacePos.xyz / max(lightSpacePos.w, 0.0001);
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float bias = max(0.0025 * (1.0 - dot(normal, lightDir)), 0.0008);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float pcfDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }

    return shadow / 25.0;
}

void main() {
    vec3 n = normalize(vNormal);
    vec3 v = normalize(uCameraPos - vWorldPos);

    vec3 dirL = normalize(uLightDir);
    vec3 dirR = reflect(-dirL, n);
    float dirDiffuseTerm = max(dot(n, dirL), 0.0);
    float dirSpecTerm = pow(max(dot(v, dirR), 0.0), uShininess);
    float shadow = computeShadow(vLightSpacePos, n, dirL);

    vec3 pointLVec = uPointLightPos - vWorldPos;
    float pointDistance = length(pointLVec);
    vec3 pointL = pointLVec / max(pointDistance, 0.0001);
    float attenuation = 1.0 / (1.0 + 0.14 * pointDistance + 0.07 * pointDistance * pointDistance);
    float pointDiffuseTerm = max(dot(n, pointL), 0.0);
    vec3 pointR = reflect(-pointL, n);
    float pointSpecTerm = pow(max(dot(v, pointR), 0.0), uShininess) * attenuation * uPointLightIntensity;

    vec3 ambient = uAmbientStrength * uBaseColor;
    vec3 dirDiffuse = 0.85 * dirDiffuseTerm * uBaseColor * (1.0 - shadow);
    vec3 dirSpecular = uSpecularStrength * dirSpecTerm * vec3(1.0) * (1.0 - shadow);

    vec3 pointDiffuse = pointDiffuseTerm * uBaseColor * uPointLightColor * attenuation * uPointLightIntensity;
    vec3 pointSpecular = uSpecularStrength * pointSpecTerm * uPointLightColor;

    vec3 color = ambient + dirDiffuse + dirSpecular + pointDiffuse + pointSpecular;
    fragColor = vec4(color, 1.0);
}
