#ifndef SHADERS_H
#define SHADERS_H

#include <GL/glew.h>
#include <iostream>

const char* vs_source = "#version 330 core\n"
"layout(location=0)in vec3 p; layout(location=1)in vec2 u; layout(location=2)in vec3 n; "
"layout(location=3)in vec3 t_in_vec; layout(location=4)in float t_in; layout(location=5)in vec3 instPos; "
"uniform mat4 m,v,pr; uniform bool isInstanced; uniform bool isCloud; uniform float time; "
"out vec2 uv; out vec3 fragPos; out float vType; out float cloudID; out mat3 TBN; "
"void main(){ "
"  vType = t_in; cloudID = float(gl_InstanceID); "
"  vec3 posOffset = instPos; "
"  if(isCloud){ "
"    float id = float(gl_InstanceID); "
"    posOffset.x += sin(time * 0.4 + id) * 300.0; "
"    posOffset.z += cos(time * 0.3 + id * 1.5) * 300.0; "
"    posOffset.y += sin(time * 0.7 + id * 2.0) * 40.0; "
"  } "
"  vec4 worldPos = isInstanced ? (m * vec4(p, 1.0) + vec4(posOffset, 0.0)) : (m * vec4(p, 1.0)); "
"  fragPos = vec3(worldPos); uv = u; "
"  vec3 T = normalize(vec3(m * vec4(t_in_vec, 0.0))); "
"  vec3 N = normalize(vec3(m * vec4(n, 0.0))); "
"  T = normalize(T - dot(T, N) * N); "
"  vec3 B = cross(N, T); "
"  TBN = mat3(T, B, N); "
"  gl_Position = pr * v * worldPos; }";

const char* fs_source = "#version 330 core\n"
"out vec4 c; in vec2 uv; in vec3 fragPos; in float vType; in float cloudID; in mat3 TBN; "
"uniform sampler2D t; uniform sampler2D nm; uniform bool useNormalMap; uniform bool useTexture; "
"uniform vec3 lightDir; uniform vec3 lanternPos[10]; uniform vec3 baseColor; uniform float time; uniform bool isCloud; "
"uniform bool spotlightOn; uniform vec3 spotlightPos; uniform vec3 spotlightDir; "
"float rand(float n){return fract(sin(n) * 43758.5453123);} "
"void main(){ "
"  vec4 tex = useTexture ? texture(t,uv) : vec4(baseColor, 1.0); "
"  if(tex.a < 0.1) discard; "
"  if(vType > 0.5) { c = vec4(1.0, 1.0, 1.0, 1.0); return; } "
"  vec3 n; if(useNormalMap) { n = texture(nm, uv).rgb; n = normalize(n * 2.0 - 1.0); n = normalize(TBN * n); } else { n = normalize(TBN[2]); } "
"  vec3 ambient = vec3(0.3, 0.3, 0.4); "
"  vec3 lighting = ambient + max(dot(n, normalize(lightDir)), 0.0) * 0.5; "
"  "
"  for(int i=0; i<10; i++){ "
"    vec3 lPos = lanternPos[i] + vec3(0, 60, 0); "
"    float dist = length(lPos - fragPos); "
"    float atten = 1.0 / (1.0 + 0.0006 * dist + 0.00002 * (dist * dist)); "
"    lighting += vec3(1.0, 0.85, 0.6) * max(dot(n, normalize(lPos - fragPos)), 0.0) * atten * 3.0; "
"  } "
"  "
"  if(spotlightOn) { "
"    vec3 toLight = normalize(spotlightPos - fragPos); "
"    float theta = dot(toLight, normalize(-spotlightDir)); "
"    float epsilon = 0.3; "
"    float intensity = clamp((theta - (1.0 - epsilon)) / epsilon, 0.0, 1.0); "
"    float distance = length(spotlightPos - fragPos); "
"    float attenuation = 1.0 / (1.0 + 0.001 * distance + 0.0001 * (distance * distance)); "
"    lighting += vec3(1.0, 0.98, 0.9) * max(dot(n, toLight), 0.0) * intensity * attenuation * 2.5; "
"  } "
"  "
"  if(isCloud){ "
"    float interval = floor(time * 1.5); "
"    float targetCloud = floor(rand(interval) * 8.0); "
"    if(abs(cloudID - targetCloud) < 0.1){ "
"       float pulse = pow(max(0.0, sin(time * 20.0)), 3.0); "
"       lighting += vec3(0.8, 0.9, 1.0) * pulse * 3.0; "
"    } "
"    float flicker = 0.9 + 0.1 * sin(time * 5.0 + cloudID * 10.0); "
"    lighting *= flicker; "
"  } "
"  "
"  c = vec4(tex.rgb * lighting, tex.a); }";

inline unsigned int CreateShaderProgram() {
    unsigned int pvs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(pvs, 1, &vs_source, 0);
    glCompileShader(pvs);
    
    int success;
    char infoLog[512];
    glGetShaderiv(pvs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(pvs, 512, NULL, infoLog);
        std::cerr << "VERTEX SHADER COMPILATION FAILED:\n" << infoLog << std::endl;
        return 0;
    }

    unsigned int pfs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(pfs, 1, &fs_source, 0);
    glCompileShader(pfs);
    
    glGetShaderiv(pfs, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(pfs, 512, NULL, infoLog);
        std::cerr << "FRAGMENT SHADER COMPILATION FAILED:\n" << infoLog << std::endl;
        return 0;
    }

    unsigned int prog = glCreateProgram();
    glAttachShader(prog, pvs);
    glAttachShader(prog, pfs);
    glLinkProgram(prog);
    
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(prog, 512, NULL, infoLog);
        std::cerr << "SHADER PROGRAM LINKING FAILED:\n" << infoLog << std::endl;
        return 0;
    }

    glDeleteShader(pvs);
    glDeleteShader(pfs);
    
    return prog;
}

#endif