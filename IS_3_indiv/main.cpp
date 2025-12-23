#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>

#include "camera.h"
#include "shaders.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

const int NUM_LANTERNS = 10;
const int NUM_HOUSES = 8;
const int NUM_TREES = 20;
const int NUM_SLEDS = 3;  

glm::vec3 lanternPositions[NUM_LANTERNS] = {
    {400.0f, 15.0f, 400.0f}, {-400.0f, 15.0f, 400.0f}, {400.0f, 15.0f, -400.0f}, {-400.0f, 15.0f, -400.0f},
    {1200.0f, 15.0f, 0.0f}, {-1200.0f, 15.0f, 0.0f}, {0.0f, 15.0f, 1200.0f}, {0.0f, 15.0f, -1200.0f},
    {1800.0f, 15.0f, 1800.0f}, {-1800.0f, 15.0f, -1800.0f}
};

glm::vec3 housePositions[NUM_HOUSES];
glm::vec3 houseColors[NUM_HOUSES];
bool houseNeedsDelivery[NUM_HOUSES];
float houseDeliveryTimers[NUM_HOUSES];
int deliveriesCompleted = 0;

glm::vec3 treePositions[NUM_TREES];

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoords;
    glm::vec3 normal;
    glm::vec3 tangent;
    float type;
};

struct GameObject {
    unsigned int vao;
    unsigned int texture;
    unsigned int normalMap;
    int vertexCount;
};

struct Sled {
    glm::vec3 position;
    float angle;
    float speed;
    float radius;
    float bobOffset;  
};

struct Package {
    glm::vec3 pos;
    glm::vec3 dir;
    float lifeTime;
    bool active;
    glm::vec3 color;
};
std::vector<Package> packages;

Sled sleds[NUM_SLEDS];

bool mouseCaptured = false;
double lastMouseX = 640.0;
double lastMouseY = 360.0;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (!mouseCaptured) return;

    Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (camera) {
        camera->ProcessMouseMovement(static_cast<float>(xpos), static_cast<float>(ypos));
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        mouseCaptured = !mouseCaptured;

        Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
        if (camera) {
            camera->ToggleMouseControl(mouseCaptured);

            if (mouseCaptured) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                camera->SetMousePosition(static_cast<float>(xpos), static_cast<float>(ypos));
                std::cout << "Mouse control ENABLED. Move mouse to look around." << std::endl;
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "Mouse control DISABLED." << std::endl;
            }
        }
    }
}

unsigned int load_texture(const char* path) {
    unsigned int id;
    glGenTextures(1, &id);
    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (data) {
        GLenum fmt = (ch == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        stbi_image_free(data);
    }
    else {
        std::cerr << "Failed to load texture: " << path << std::endl;
        unsigned char white[] = { 255, 255, 255, 255 };
        glBindTexture(GL_TEXTURE_2D, id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    }
    return id;
}

void computeTangents(std::vector<Vertex>& out) {
    for (size_t i = 0; i < out.size(); i += 3) {
        if (i + 2 >= out.size()) break;
        glm::vec3& v0 = out[i].position;
        glm::vec3& v1 = out[i + 1].position;
        glm::vec3& v2 = out[i + 2].position;
        glm::vec2& uv0 = out[i].texCoords;
        glm::vec2& uv1 = out[i + 1].texCoords;
        glm::vec2& uv2 = out[i + 2].texCoords;

        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);

        glm::vec3 tangent;
        tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
        tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
        tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);

        tangent = glm::normalize(tangent);
        out[i].tangent = out[i + 1].tangent = out[i + 2].tangent = tangent;
    }
}

void load_obj(const std::string& path, std::vector<Vertex>& out) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec3> normals;

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to load OBJ file: " << path << ". Using fallback cube." << std::endl;
        float s = 1.0f;
        positions = {
            {-s,-s,-s}, {s,-s,-s}, {s,s,-s}, {-s,s,-s},
            {-s,-s,s}, {s,-s,s}, {s,s,s}, {-s,s,s}
        };

        int indices[] = {
            0,1,2, 0,2,3, 4,5,6, 4,6,7,
            0,1,5, 0,5,4, 1,2,6, 1,6,5,
            2,3,7, 2,7,6, 3,0,4, 3,4,7
        };

        for (int i = 0; i < 36; i++) {
            Vertex v;
            v.position = positions[indices[i]];
            v.texCoords = glm::vec2(0.0f, 0.0f);
            v.normal = glm::normalize(v.position);
            v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            v.type = 0.0f;
            out.push_back(v);
        }
        computeTangents(out);
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            glm::vec3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
        }
        else if (type == "vt") {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y;
            texcoords.push_back(uv);
        }
        else if (type == "vn") {
            glm::vec3 normal;
            ss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (type == "f") {
            std::string v1, v2, v3;
            ss >> v1 >> v2 >> v3;

            auto parseFace = [&](const std::string& vertex) {
                std::stringstream vss(vertex);
                std::string indices[3];
                for (int i = 0; i < 3; i++) {
                    std::getline(vss, indices[i], '/');
                }

                int posIdx = std::stoi(indices[0]) - 1;
                int texIdx = indices[1].empty() ? 0 : std::stoi(indices[1]) - 1;
                int normIdx = indices[2].empty() ? 0 : std::stoi(indices[2]) - 1;

                Vertex v;
                v.position = positions[posIdx];
                v.texCoords = texIdx < texcoords.size() ? texcoords[texIdx] : glm::vec2(0.0f);
                v.normal = normIdx < normals.size() ? normals[normIdx] : glm::vec3(0.0f, 1.0f, 0.0f);
                v.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                v.type = 0.0f;

                return v;
                };

            out.push_back(parseFace(v1));
            out.push_back(parseFace(v2));
            out.push_back(parseFace(v3));
        }
    }

    file.close();
    std::cout << "Loaded OBJ file: " << path << " with " << out.size() << " vertices" << std::endl;
    computeTangents(out);
}

void generateTerrain(std::vector<Vertex>& out) {
    int width = 50, height = 50;
    float size = 5000.0f;
    float maxHeight = 100.0f;

    std::vector<std::vector<glm::vec3>> posMap(height, std::vector<glm::vec3>(width));

    for (int z = 0; z < height; z++) {
        for (int x = 0; x < width; x++) {
            float nx = static_cast<float>(x) / static_cast<float>(width) * 4.0f;
            float nz = static_cast<float>(z) / static_cast<float>(height) * 4.0f;
            float h = 0.5f + 0.3f * sin(nx) * cos(nz);

            posMap[z][x] = glm::vec3(
                (static_cast<float>(x) / static_cast<float>(width) - 0.5f) * size,
                h * maxHeight * 0.3f,
                (static_cast<float>(z) / static_cast<float>(height) - 0.5f) * size
            );
        }
    }

    for (int z = 0; z < height - 1; z++) {
        for (int x = 0; x < width - 1; x++) {
            glm::vec2 uv0(static_cast<float>(x) / static_cast<float>(width), static_cast<float>(z) / static_cast<float>(height));
            glm::vec2 uv1(static_cast<float>(x) / static_cast<float>(width), static_cast<float>(z + 1) / static_cast<float>(height));
            glm::vec2 uv2(static_cast<float>(x + 1) / static_cast<float>(width), static_cast<float>(z) / static_cast<float>(height));
            glm::vec2 uv3(static_cast<float>(x + 1) / static_cast<float>(width), static_cast<float>(z + 1) / static_cast<float>(height));

            out.push_back({ posMap[z][x], uv0 * 20.0f, {0.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f}, 0.0f });
            out.push_back({ posMap[z + 1][x], uv1 * 20.0f, {0.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f}, 0.0f });
            out.push_back({ posMap[z][x + 1], uv2 * 20.0f, {0.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f}, 0.0f });

            out.push_back({ posMap[z][x + 1], uv2 * 20.0f, {0.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f}, 0.0f });
            out.push_back({ posMap[z + 1][x], uv1 * 20.0f, {0.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f}, 0.0f });
            out.push_back({ posMap[z + 1][x + 1], uv3 * 20.0f, {0.0f,1.0f,0.0f}, {0.0f,0.0f,0.0f}, 0.0f });
        }
    }
    computeTangents(out);
}

void generateSphere(std::vector<Vertex>& out, float radius, int sectors, int stacks, float type) {
    std::vector<Vertex> raw;
    for (int i = 0; i <= stacks; ++i) {
        float stackAngle = M_PI / 2.0f - static_cast<float>(i) / static_cast<float>(stacks) * M_PI;
        float xy = radius * cosf(stackAngle);
        float z = radius * sinf(stackAngle);
        for (int j = 0; j <= sectors; ++j) {
            float sectorAngle = static_cast<float>(j) / static_cast<float>(sectors) * 2.0f * M_PI;
            glm::vec3 pos(xy * cosf(sectorAngle), z, xy * sinf(sectorAngle));
            raw.push_back({ pos,
                           glm::vec2(static_cast<float>(j) / static_cast<float>(sectors),
                                     static_cast<float>(i) / static_cast<float>(stacks)),
                           glm::normalize(pos),
                           {0.0f,0.0f,0.0f},
                           type });
        }
    }

    std::vector<Vertex> tris;
    for (int i = 0; i < stacks; ++i) {
        int k1 = i * (sectors + 1);
        int k2 = k1 + sectors + 1;
        for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                tris.push_back(raw[k1]);
                tris.push_back(raw[k2]);
                tris.push_back(raw[k1 + 1]);
            }
            if (i != (stacks - 1)) {
                tris.push_back(raw[k1 + 1]);
                tris.push_back(raw[k2]);
                tris.push_back(raw[k2 + 1]);
            }
        }
    }
    computeTangents(tris);
    out.insert(out.end(), tris.begin(), tris.end());
}

void generateHouse(std::vector<Vertex>& out) {
    float w = 0.5f, h = 0.5f, d = 0.5f;

    glm::vec3 vertices[] = {
        {-w, -h,  d}, { w, -h,  d}, { w,  h,  d}, {-w,  h,  d},
        {-w, -h, -d}, { w, -h, -d}, { w,  h, -d}, {-w,  h, -d}
    };

    int indices[] = {
        0,1,2, 0,2,3,
        5,4,7, 5,7,6,
        1,5,6, 1,6,2,
        4,0,3, 4,3,7,
        3,2,6, 3,6,7,
        4,5,1, 4,1,0
    };

    for (int i = 0; i < 36; i += 3) {
        int idx1 = indices[i], idx2 = indices[i + 1], idx3 = indices[i + 2];

        Vertex v1, v2, v3;
        v1.position = vertices[idx1];
        v2.position = vertices[idx2];
        v3.position = vertices[idx3];

        v1.texCoords = glm::vec2(0.0f, 0.0f);
        v2.texCoords = glm::vec2(1.0f, 0.0f);
        v3.texCoords = glm::vec2(0.5f, 1.0f);

        glm::vec3 edge1 = v2.position - v1.position;
        glm::vec3 edge2 = v3.position - v1.position;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        v1.normal = v2.normal = v3.normal = normal;
        v1.type = v2.type = v3.type = 0.0f;
        v1.tangent = v2.tangent = v3.tangent = glm::vec3(1.0f, 0.0f, 0.0f);

        out.push_back(v1);
        out.push_back(v2);
        out.push_back(v3);
    }

    glm::vec3 roofVertices[] = {
        {-w * 1.3f, h, -d * 1.3f},
        { w * 1.3f, h, -d * 1.3f},
        { w * 1.3f, h,  d * 1.3f},
        {-w * 1.3f, h,  d * 1.3f},
        {0.0f, h + 0.6f, 0.0f}
    };

    int roofIndices[] = {
        0,1,4, 1,2,4, 2,3,4, 3,0,4
    };

    for (int i = 0; i < 12; i += 3) {
        int idx1 = roofIndices[i], idx2 = roofIndices[i + 1], idx3 = roofIndices[i + 2];

        Vertex v1, v2, v3;
        v1.position = roofVertices[idx1];
        v2.position = roofVertices[idx2];
        v3.position = roofVertices[idx3];

        v1.texCoords = glm::vec2(0.0f, 0.0f);
        v2.texCoords = glm::vec2(1.0f, 0.0f);
        v3.texCoords = glm::vec2(0.5f, 1.0f);

        glm::vec3 edge1 = v2.position - v1.position;
        glm::vec3 edge2 = v3.position - v1.position;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        v1.normal = v2.normal = v3.normal = normal;
        v1.type = v2.type = v3.type = 0.0f;
        v1.tangent = v2.tangent = v3.tangent = glm::vec3(1.0f, 0.0f, 0.0f);

        out.push_back(v1);
        out.push_back(v2);
        out.push_back(v3);
    }

    computeTangents(out);
}

void generateTree(std::vector<Vertex>& out) {
    float trunkHeight = 1.2f;
    float trunkRadius = 0.1f;
    int segments = 8;

    for (int i = 0; i < segments; i++) {
        float a1 = 2.0f * M_PI * i / segments;
        float a2 = 2.0f * M_PI * (i + 1) / segments;

        glm::vec3 p1(cos(a1) * trunkRadius, -0.5f, sin(a1) * trunkRadius);
        glm::vec3 p2(cos(a2) * trunkRadius, -0.5f, sin(a2) * trunkRadius);
        glm::vec3 p3(cos(a1) * trunkRadius, trunkHeight, sin(a1) * trunkRadius);
        glm::vec3 p4(cos(a2) * trunkRadius, trunkHeight, sin(a2) * trunkRadius);

        out.push_back({ p1, {0.0f, 0.0f}, glm::normalize(glm::vec3(p1.x, 0.0f, p1.z)), {0.0f,0.0f,0.0f}, 0.0f });
        out.push_back({ p2, {1.0f, 0.0f}, glm::normalize(glm::vec3(p2.x, 0.0f, p2.z)), {0.0f,0.0f,0.0f}, 0.0f });
        out.push_back({ p3, {0.0f, 1.0f}, glm::normalize(glm::vec3(p1.x, 0.0f, p1.z)), {0.0f,0.0f,0.0f}, 0.0f });

        out.push_back({ p2, {1.0f, 0.0f}, glm::normalize(glm::vec3(p2.x, 0.0f, p2.z)), {0.0f,0.0f,0.0f}, 0.0f });
        out.push_back({ p4, {1.0f, 1.0f}, glm::normalize(glm::vec3(p2.x, 0.0f, p2.z)), {0.0f,0.0f,0.0f}, 0.0f });
        out.push_back({ p3, {0.0f, 1.0f}, glm::normalize(glm::vec3(p1.x, 0.0f, p1.z)), {0.0f,0.0f,0.0f}, 0.0f });
    }

    std::vector<Vertex> crown1, crown2;
    generateSphere(crown1, 0.5f, 8, 6, 1.0f);
    generateSphere(crown2, 0.3f, 8, 6, 1.0f);

    for (auto& v : crown1) { v.position.y += trunkHeight - 0.1f; out.push_back(v); }
    for (auto& v : crown2) { v.position.y += trunkHeight + 0.3f; out.push_back(v); }

    computeTangents(out);
}

void generateLantern(std::vector<Vertex>& out) {
    float r = 2.0f, h = 40.0f;

    for (int i = 0; i < 12; i++) {
        float a = 2.0f * M_PI * i / 12.0f;
        float na = 2.0f * M_PI * (i + 1) / 12.0f;

        glm::vec3 n(cos(a), 0.0f, sin(a));

        out.push_back({ {cos(a) * r, 0.0f, sin(a) * r}, {0.0f,0.0f}, n, {0.0f,0.0f,0.0f}, 0.0f });
        out.push_back({ {cos(na) * r, 0.0f, sin(na) * r}, {1.0f,0.0f}, n, {0.0f,0.0f,0.0f}, 0.0f });
        out.push_back({ {cos(a) * r, h, sin(a) * r}, {0.0f,1.0f}, n, {0.0f,0.0f,0.0f}, 0.0f });

        out.push_back({ {cos(na) * r, 0.0f, sin(na) * r}, {1.0f,0.0f}, n, {0.0f,0.0f,0.0f}, 0.0f });
        out.push_back({ {cos(na) * r, h, sin(na) * r}, {1.0f,1.0f}, n, {0.0f,0.0f,0.0f}, 0.0f });
        out.push_back({ {cos(a) * r, h, sin(a) * r}, {0.0f,1.0f}, n, {0.0f,0.0f,0.0f}, 0.0f });
    }

    computeTangents(out);

    std::vector<Vertex> bulb;
    generateSphere(bulb, 4.0f, 10, 10, 1.0f);
    for (auto& v : bulb) {
        v.position.y += h;
        out.push_back(v);
    }
}

void generateSled(std::vector<Vertex>& out) {
    float length = 1.0f;
    float width = 0.3f;
    float height = 0.15f;

    glm::vec3 vertices[] = {
        {-length / 2, height / 2, -width / 2},
        { length / 2, height / 2, -width / 2},
        { length / 2, height / 2,  width / 2},
        {-length / 2, height / 2,  width / 2},

        {-length / 2, height, -width / 2},
        { length / 2, height, -width / 2},
        { length / 2, height,  width / 2},
        {-length / 2, height,  width / 2},

        {-length / 2 * 0.7f, 0.0f, -width / 2},
        { length / 2 * 0.7f, 0.0f, -width / 2},
        { length / 2 * 0.7f, 0.0f,  width / 2},
        {-length / 2 * 0.7f, 0.0f,  width / 2},

        {0.0f, height * 0.5f, -width / 2},
        {0.0f, height * 0.5f, width / 2}
    };

    int indices[] = {
        0,1,2, 0,2,3,

        0,4,5, 0,5,1,
        1,5,6, 1,6,2,
        2,6,7, 2,7,3,
        3,7,4, 3,4,0,

        4,5,6, 4,6,7,

        8,9,10, 8,10,11,

        0,8,11, 0,11,3,
        1,9,8, 1,8,0,
        2,10,9, 2,9,1,
        3,11,10, 3,10,2,

        8,12,9, 11,13,10
    };

    for (int i = 0; i < 36; i += 3) {
        int idx1 = indices[i], idx2 = indices[i + 1], idx3 = indices[i + 2];

        Vertex v1, v2, v3;
        v1.position = vertices[idx1];
        v2.position = vertices[idx2];
        v3.position = vertices[idx3];

        v1.texCoords = glm::vec2(0.0f, 0.0f);
        v2.texCoords = glm::vec2(1.0f, 0.0f);
        v3.texCoords = glm::vec2(0.5f, 1.0f);

        glm::vec3 edge1 = v2.position - v1.position;
        glm::vec3 edge2 = v3.position - v1.position;
        glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

        v1.normal = v2.normal = v3.normal = normal;
        v1.tangent = v2.tangent = v3.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        v1.type = v2.type = v3.type = 0.0f;

        out.push_back(v1);
        out.push_back(v2);
        out.push_back(v3);
    }

    computeTangents(out);
}

void generateSnowCircle(std::vector<Vertex>& out) {
    float radius = 200.0f;
    int segments = 32;
    float centerY = 0.0f;

    glm::vec3 center(0.0f, centerY, 200.0f);

    for (int i = 0; i < segments; i++) {
        float angle1 = 2.0f * M_PI * i / segments;
        float angle2 = 2.0f * M_PI * (i + 1) / segments;

        glm::vec3 p1 = center + glm::vec3(sin(angle1) * radius, 0.0f, cos(angle1) * radius);
        glm::vec3 p2 = center + glm::vec3(sin(angle2) * radius, 0.0f, cos(angle2) * radius);
        glm::vec3 p3 = center;

        p3.y = centerY + 5.0f;

        Vertex v1, v2, v3;
        v1.position = p1;
        v2.position = p2;
        v3.position = p3;

        v1.texCoords = glm::vec2((sin(angle1) + 1.0f) * 0.5f, (cos(angle1) + 1.0f) * 0.5f);
        v2.texCoords = glm::vec2((sin(angle2) + 1.0f) * 0.5f, (cos(angle2) + 1.0f) * 0.5f);
        v3.texCoords = glm::vec2(0.5f, 0.5f);

        v1.normal = v2.normal = v3.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v1.tangent = v2.tangent = v3.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        v1.type = v2.type = v3.type = 0.0f;

        out.push_back(v1);
        out.push_back(v2);
        out.push_back(v3);
    }

    computeTangents(out);
}

GameObject create_obj(const std::string& type, const std::string& png = "", const std::string& nmap = "",
    glm::vec3* instPos = nullptr, int instCount = 0, float sType = 0.0f) {
    std::vector<Vertex> vertices;

    if (type == "GEN_TERRAIN") {
        generateTerrain(vertices);
    }
    else if (type == "GEN_HOUSE") {
        generateHouse(vertices);
    }
    else if (type == "GEN_TREE") {
        generateTree(vertices);
    }
    else if (type == "GEN_LANTERN") {
        generateLantern(vertices);
    }
    else if (type == "GEN_SPHERE") {
        generateSphere(vertices, 1.0f, 12, 12, sType);
    }
    else if (type == "GEN_SLED") {
        generateSled(vertices);
    }
    else if (type == "GEN_SNOW_CIRCLE") {
        generateSnowCircle(vertices);
    }
    else {
        load_obj(type, vertices);
    }

    unsigned int vao, vbo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, type));

    if (instPos != nullptr && instCount > 0) {
        unsigned int instanceVBO;
        glGenBuffers(1, &instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instCount * sizeof(glm::vec3), instPos, GL_STATIC_DRAW);

        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glVertexAttribDivisor(5, 1);
    }

    glBindVertexArray(0);

    unsigned int texture = 0;
    if (!png.empty()) {
        texture = load_texture(png.c_str());
    }

    unsigned int normalMap = 0;
    if (!nmap.empty()) {
        normalMap = load_texture(nmap.c_str());
    }

    return { vao, texture, normalMap, static_cast<int>(vertices.size()) };
}

int main() {
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    for (int i = 0; i < NUM_HOUSES; i++) {
        housePositions[i] = glm::vec3(
            static_cast<float>(std::rand() % 4000 - 2000),
            15.0f,
            static_cast<float>(std::rand() % 4000 - 2000)
        );

        houseColors[i] = glm::vec3(
            static_cast<float>(std::rand() % 70 + 30) / 100.0f,
            static_cast<float>(std::rand() % 70 + 30) / 100.0f,
            static_cast<float>(std::rand() % 70 + 30) / 100.0f
        );

        houseNeedsDelivery[i] = true;
        houseDeliveryTimers[i] = static_cast<float>(std::rand() % 10 + 5);
    }

    for (int i = 0; i < NUM_TREES; i++) {
        treePositions[i] = glm::vec3(
            static_cast<float>(std::rand() % 6000 - 3000),
            0.0f,
            static_cast<float>(std::rand() % 6000 - 3000)
        );
    }

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Zima", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

    unsigned int program = CreateShaderProgram();
    if (program == 0) {
        std::cerr << "Failed to create shader program" << std::endl;
        return -1;
    }

    glUseProgram(program);

    std::cout << "Creating winter scene with sleds circling the Christmas tree..." << std::endl;

    GameObject terrain = create_obj("GEN_TERRAIN", "Field.png", "", nullptr, 0, 0.0f);
    GameObject airship = create_obj("shar.obj", "shar.png", "shar_displacement.png", nullptr, 0, 0.0f);
    GameObject tree = create_obj("ChrTree.obj", "ChrTree.png", "", nullptr, 0, 0.0f);
    GameObject lantern = create_obj("GEN_LANTERN", "", "", lanternPositions, NUM_LANTERNS, 0.0f);
    GameObject houseObj = create_obj("GEN_HOUSE", "", "", nullptr, 0, 0.0f);
    GameObject packageObj = create_obj("GEN_SPHERE", "", "", nullptr, 0, 1.0f);
    GameObject treeInstanced = create_obj("GEN_TREE", "", "", treePositions, NUM_TREES, 0.0f);

    GameObject snowCircle = create_obj("GEN_SNOW_CIRCLE", "", "", nullptr, 0, 0.0f);

    GameObject sledObj = create_obj("GEN_SLED", "", "", nullptr, 0, 0.0f);

    try {
        GameObject sledObjLoaded = create_obj("sled.obj", "", "", nullptr, 0, 0.0f);
        if (sledObjLoaded.vao != 0) {
            std::cout << "Successfully loaded sled OBJ model" << std::endl;
            sledObj = sledObjLoaded;
        }
    }
    catch (...) {
        std::cout << "Could not load sled OBJ file. Using generated model." << std::endl;
    }

    for (int i = 0; i < NUM_SLEDS; i++) {
        float radius = 120.0f + static_cast<float>(i) * 40.0f;  
        float angle = static_cast<float>(i) * (2.0f * M_PI / NUM_SLEDS);

        sleds[i].radius = radius;
        sleds[i].angle = angle;
        sleds[i].speed = 0.3f + static_cast<float>(i) * 0.15f;  
        sleds[i].bobOffset = static_cast<float>(std::rand() % 100) / 100.0f * 2.0f * M_PI;  

        sleds[i].position = glm::vec3(
            sin(angle) * radius,
            20.0f, 
            cos(angle) * radius + 200.0f  
        );
    }

    int modelLoc = glGetUniformLocation(program, "m");
    int viewLoc = glGetUniformLocation(program, "v");
    int projectionLoc = glGetUniformLocation(program, "pr");
    int useTextureLoc = glGetUniformLocation(program, "useTexture");
    int lightDirLoc = glGetUniformLocation(program, "lightDir");
    int lanternPosLoc = glGetUniformLocation(program, "lanternPos");
    int isInstancedLoc = glGetUniformLocation(program, "isInstanced");
    int baseColorLoc = glGetUniformLocation(program, "baseColor");
    int timeLoc = glGetUniformLocation(program, "time");
    int isCloudLoc = glGetUniformLocation(program, "isCloud");
    int useNormalMapLoc = glGetUniformLocation(program, "useNormalMap");
    int spotlightOnLoc = glGetUniformLocation(program, "spotlightOn");
    int spotlightPosLoc = glGetUniformLocation(program, "spotlightPos");
    int spotlightDirLoc = glGetUniformLocation(program, "spotlightDir");

    if (spotlightOnLoc == -1) std::cerr << "Warning: spotlightOn uniform not found" << std::endl;
    if (spotlightPosLoc == -1) std::cerr << "Warning: spotlightPos uniform not found" << std::endl;
    if (spotlightDirLoc == -1) std::cerr << "Warning: spotlightDir uniform not found" << std::endl;

    glUniform1i(glGetUniformLocation(program, "t"), 0);
    glUniform1i(glGetUniformLocation(program, "nm"), 1);

    Camera camera;
    glfwSetWindowUserPointer(window, &camera);

    glm::vec3 airshipPos(0.0f, 300.0f, 0.0f);
    float lastFrame = 0.0f;

    bool isAimMode = false;
    bool cPressed = false;
    bool enterPressed = false;
    bool spotlightOn = false;
    bool fPressed = false;
    bool showInfo = true;
    bool mPressed = false;  

    float gameTime = 0.0f;
    int score = 0;

    std::cout << "Starting winter game with mouse control..." << std::endl;
    std::cout << "=== CONTROLS ===" << std::endl;
    std::cout << "RIGHT MOUSE BUTTON - toggle mouse look" << std::endl;
    std::cout << "WASD - movement" << std::endl;
    std::cout << "Space/Shift - altitude" << std::endl;
    std::cout << "Arrows - camera (when mouse disabled)" << std::endl;
    std::cout << "C - toggle view mode (aim/overview)" << std::endl;
    std::cout << "F - toggle spotlight" << std::endl;
    std::cout << "Enter - drop package" << std::endl;
    std::cout << "M - alternative toggle mouse control" << std::endl;
    std::cout << "ESC - exit" << std::endl;
    std::cout << "=================" << std::endl;

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        gameTime += deltaTime;

        for (int i = 0; i < NUM_SLEDS; i++) {
            sleds[i].angle += sleds[i].speed * deltaTime;

            sleds[i].position.x = sin(sleds[i].angle) * sleds[i].radius;
            sleds[i].position.z = cos(sleds[i].angle) * sleds[i].radius + 200.0f;

            sleds[i].position.y = 20.0f + sin(gameTime * 2.0f + sleds[i].bobOffset) * 3.0f;
        }

        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !cPressed) {
            isAimMode = !isAimMode;
            cPressed = true;
            camera.pitch = isAimMode ? -10.0f : 25.0f;
            std::cout << "Camera mode: " << (isAimMode ? "aiming" : "overview") << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) cPressed = false;

        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !fPressed) {
            spotlightOn = !spotlightOn;
            fPressed = true;
            std::cout << "Spotlight: " << (spotlightOn ? "ON" : "OFF") << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) fPressed = false;

        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && !enterPressed) {
            Package newPackage;
            newPackage.pos = isAimMode ? airshipPos + glm::vec3(0.0f, 20.0f, 0.0f) : airshipPos;

            glm::vec3 shotDir = camera.GetForward();
            if (!isAimMode) shotDir = -shotDir;

            newPackage.dir = glm::normalize(shotDir);
            newPackage.lifeTime = 6.0f;
            newPackage.active = true;
            newPackage.color = glm::vec3(1.0f, 0.9f, 0.3f);

            packages.push_back(newPackage);
            enterPressed = true;

            std::cout << "Package dropped! Total packages: " << packages.size() << std::endl;
        }
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_RELEASE) enterPressed = false;

        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS && !mPressed) {
            mouseCaptured = !mouseCaptured;
            camera.ToggleMouseControl(mouseCaptured);

            if (mouseCaptured) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                std::cout << "Mouse control ENABLED (via M key)" << std::endl;
            }
            else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                std::cout << "Mouse control DISABLED (via M key)" << std::endl;
            }
            mPressed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_M) == GLFW_RELEASE) mPressed = false;

        if (!mouseCaptured) {
            float lookSpeed = 80.0f * deltaTime;
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    camera.pitch += lookSpeed;
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  camera.pitch -= lookSpeed;
            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  camera.yaw += lookSpeed;
            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) camera.yaw -= lookSpeed;

            camera.pitch = std::max(-89.0f, std::min(89.0f, camera.pitch));
        }

        for (auto& pkg : packages) {
            if (!pkg.active) continue;

            pkg.pos += pkg.dir * 600.0f * deltaTime;
            pkg.lifeTime -= deltaTime;

            if (pkg.lifeTime <= 0) {
                pkg.active = false;
                continue;
            }

            for (int i = 0; i < NUM_HOUSES; i++) {
                if (houseNeedsDelivery[i] && glm::distance(pkg.pos, housePositions[i]) < 40.0f) {
                    houseNeedsDelivery[i] = false;
                    pkg.active = false;
                    score += 10;
                    deliveriesCompleted++;

                    std::cout << "Hit house " << i << "! Score: " << score;
                    std::cout << " Deliveries: " << deliveriesCompleted << "/" << NUM_HOUSES << std::endl;
                    break;
                }
            }

            if (pkg.pos.y < 30.0f) {
                pkg.active = false;
            }
        }

        packages.erase(std::remove_if(packages.begin(), packages.end(),
            [](const Package& p) { return !p.active; }), packages.end());

        for (int i = 0; i < NUM_HOUSES; i++) {
            if (!houseNeedsDelivery[i]) {
                houseDeliveryTimers[i] -= deltaTime;
                if (houseDeliveryTimers[i] <= 0) {
                    houseNeedsDelivery[i] = true;
                    houseDeliveryTimers[i] = static_cast<float>(std::rand() % 10 + 8);
                    std::cout << "House " << i << " needs delivery again!" << std::endl;
                }
            }
        }

        float moveSpeed = 400.0f * deltaTime;
        glm::vec3 camForward = camera.GetForward();
        if (!isAimMode) camForward = -camForward;

        glm::vec3 flatForward = glm::normalize(glm::vec3(camForward.x, 0.0f, camForward.z));
        glm::vec3 flatRight = glm::normalize(glm::cross(flatForward, glm::vec3(0.0f, 1.0f, 0.0f)));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) airshipPos += flatForward * moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) airshipPos -= flatForward * moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) airshipPos -= flatRight * moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) airshipPos += flatRight * moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) airshipPos.y += moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) airshipPos.y -= moveSpeed;

        airshipPos.y = std::max(50.0f, std::min(1000.0f, airshipPos.y));

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(0.05f, 0.08f, 0.12f, 1.0f); 

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 1.0f, 15000.0f);
        glm::mat4 view = isAimMode ? camera.GetViewAim(airshipPos) : camera.GetView(airshipPos);

        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

        glm::vec3 lightDirection = glm::normalize(glm::vec3(0.2f, -0.4f, 0.2f));
        glUniform3f(lightDirLoc, lightDirection.x, lightDirection.y, lightDirection.z);
        glUniform3fv(lanternPosLoc, NUM_LANTERNS, glm::value_ptr(lanternPositions[0]));
        glUniform1f(timeLoc, gameTime);

        if (spotlightOnLoc != -1) glUniform1i(spotlightOnLoc, spotlightOn ? 1 : 0);
        if (spotlightPosLoc != -1) glUniform3f(spotlightPosLoc, airshipPos.x, airshipPos.y, airshipPos.z);
        if (spotlightDirLoc != -1) {
            glm::vec3 spotDir = camera.GetForward();
            if (!isAimMode) spotDir = -spotDir;
            glUniform3f(spotlightDirLoc, spotDir.x, spotDir.y, spotDir.z);
        }

        glUniform1i(isCloudLoc, 0);
        glUniform1i(isInstancedLoc, 0);
        glUniform1i(useTextureLoc, 1);
        glUniform1i(useNormalMapLoc, 0);

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, terrain.texture);
        glBindVertexArray(terrain.vao);
        glDrawArrays(GL_TRIANGLES, 0, terrain.vertexCount); 

        glUniform1i(useTextureLoc, 0);
        glUniform3f(baseColorLoc, 0.95f, 0.97f, 1.0f);  
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glBindVertexArray(snowCircle.vao);
        glDrawArrays(GL_TRIANGLES, 0, snowCircle.vertexCount);  

        glUniform1i(useTextureLoc, 1);
        glm::mat4 treeModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 200.0f));
        treeModel = glm::rotate(treeModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        treeModel = glm::scale(treeModel, glm::vec3(400.0f, 400.0f, 400.0f));
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(treeModel));
        glBindTexture(GL_TEXTURE_2D, tree.texture);
        glBindVertexArray(tree.vao);
        glDrawArrays(GL_TRIANGLES, 0, tree.vertexCount); 

        glUniform1i(isInstancedLoc, 1);
        glUniform1i(useTextureLoc, 0);
        glUniform3f(baseColorLoc, 0.9f, 0.9f, 0.8f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glBindVertexArray(lantern.vao);
        glDrawArraysInstanced(GL_TRIANGLES, 0, lantern.vertexCount, NUM_LANTERNS);  

        glUniform1i(isInstancedLoc, 0);
        glUniform3f(baseColorLoc, 0.7f, 0.7f, 0.7f);

        for (int i = 0; i < NUM_HOUSES; i++) {
            glm::mat4 houseModel = glm::translate(glm::mat4(1.0f), housePositions[i]);
            houseModel = glm::scale(houseModel, glm::vec3(30.0f, 30.0f, 30.0f));

            if (houseNeedsDelivery[i]) {
                glUniform3f(baseColorLoc, houseColors[i].r, houseColors[i].g, houseColors[i].b);
            }
            else {
                glUniform3f(baseColorLoc, 0.4f, 0.4f, 0.4f);
            }

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(houseModel));
            glBindVertexArray(houseObj.vao);
            glDrawArrays(GL_TRIANGLES, 0, houseObj.vertexCount);  
        }

        glUniform1i(isInstancedLoc, 1);
        glUniform3f(baseColorLoc, 0.3f, 0.6f, 0.2f);
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
        glBindVertexArray(treeInstanced.vao);
        glDrawArraysInstanced(GL_TRIANGLES, 0, treeInstanced.vertexCount, NUM_TREES);  

        glUniform1i(isInstancedLoc, 0);
        glBindVertexArray(packageObj.vao);

        for (const auto& pkg : packages) {
            if (!pkg.active) continue;

            glm::mat4 packageModel = glm::translate(glm::mat4(1.0f), pkg.pos);
            packageModel = glm::scale(packageModel, glm::vec3(6.0f, 6.0f, 6.0f));

            float pulse = 0.8f + 0.2f * sin(gameTime * 8.0f);
            glUniform3f(baseColorLoc, pkg.color.r * pulse, pkg.color.g * pulse, pkg.color.b * pulse);

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(packageModel));
            glDrawArrays(GL_TRIANGLES, 0, packageObj.vertexCount);  
        }

        glUniform1i(useTextureLoc, 0);

        for (int i = 0; i < NUM_SLEDS; i++) {
            glm::vec3 sledColor;
            switch (i % 3) {
            case 0: sledColor = glm::vec3(0.8f, 0.2f, 0.2f); break;  
            case 1: sledColor = glm::vec3(0.2f, 0.8f, 0.2f); break;  
            case 2: sledColor = glm::vec3(0.2f, 0.2f, 0.8f); break;  
            }

            glUniform3f(baseColorLoc, sledColor.r, sledColor.g, sledColor.b);

            glm::mat4 sledModel = glm::translate(glm::mat4(1.0f), sleds[i].position);

            float rotationAngle = sleds[i].angle + M_PI;
            sledModel = glm::rotate(sledModel, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));

            sledModel = glm::rotate(sledModel, sin(gameTime + sleds[i].bobOffset) * 0.1f, glm::vec3(0.0f, 0.0f, 1.0f));

            sledModel = glm::scale(sledModel, glm::vec3(2.0f, 2.0f, 2.0f));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(sledModel));
            glBindVertexArray(sledObj.vao);
            glDrawArrays(GL_TRIANGLES, 0, sledObj.vertexCount);  
        }

        if (!isAimMode) {
            glUniform1i(useTextureLoc, 1);
            glUniform1i(useNormalMapLoc, 1);
            glUniform3f(baseColorLoc, 1.0f, 1.0f, 1.0f);

            glm::mat4 airshipModel = glm::translate(glm::mat4(1.0f), airshipPos);
            airshipModel = glm::rotate(airshipModel, glm::radians(camera.yaw), glm::vec3(0.0f, 1.0f, 0.0f));
            airshipModel = glm::rotate(airshipModel, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
            airshipModel = glm::scale(airshipModel, glm::vec3(2.0f, 2.0f, 2.0f));

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(airshipModel));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, airship.texture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, airship.normalMap);
            glBindVertexArray(airship.vao);
            glDrawArrays(GL_TRIANGLES, 0, airship.vertexCount); 
        }

        if (showInfo && gameTime > 5.0f && gameTime < 5.1f) {
            std::cout << "\n=== WINTER AIRSHIP DELIVERY ===\n";
            std::cout << "Score: " << score << " | Deliveries: " << deliveriesCompleted << "/" << NUM_HOUSES << "\n";
            std::cout << "Time: " << static_cast<int>(gameTime) << " sec\n";
            std::cout << "Active packages: " << packages.size() << "\n";
            std::cout << "Sleds circling the tree: " << NUM_SLEDS << "\n";
            std::cout << "Airship position: (" << static_cast<int>(airshipPos.x) << ", "
                << static_cast<int>(airshipPos.y) << ", " << static_cast<int>(airshipPos.z) << ")\n";
            std::cout << "Camera angles: pitch=" << camera.pitch << ", yaw=" << camera.yaw << "\n";
            std::cout << "Mouse control: " << (mouseCaptured ? "ENABLED" : "DISABLED") << "\n";
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }
    }

    std::cout << "\n=== GAME OVER ===\n";
    std::cout << "Final score: " << score << " points\n";
    std::cout << "Deliveries completed: " << deliveriesCompleted << " of " << NUM_HOUSES << "\n";
    std::cout << "Total time: " << static_cast<int>(gameTime) << " seconds\n";
    std::cout << "Sleds completed their circles!\n";

    glfwTerminate();
    return 0;
}