#include <GLES/gl.h>

#include <iostream>
#include <mods/chat/chat.h>
#include <mods/misc/misc.h>
#include <symbols/minecraft.h>

/// Needed symbols
typedef unsigned char uchar;

typedef uchar *(*Mob_t)(uchar *mob, uchar *level);
static Mob_t Mob = (Mob_t)0x81b80;
static uint32_t Minecraft_camera_property_offset = 0x194; // Entity
typedef uchar *(*Level_addEntity_t)(uchar *level, uchar *entity);
static Level_addEntity_t Level_addEntity = (Level_addEntity_t)0xa7cbc;
#define CAMERA_ENTITY_SIZE 0xbe4
static uchar *CameraEntity_vtable = (uchar *)0x108898;
static uint32_t CameraEntity_tracking_property_offset = 0xbe0; // int
static uint32_t Entity_hitboxWidth_property_offset = 0x70;     // float
static uint32_t Entity_headHeight_property_offset = 0x68;      // float
static uint32_t Entity_hitboxHeight_property_offset = 0x6c;    // float
typedef void (*Entity_moveTo_t)(uchar *entity, float x, float y, float z, float pitch, float yaw);
static Entity_moveTo_t Entity_moveTo = (Entity_moveTo_t)0x7a834;

static unsigned char *color_buffer = nullptr;
static std::vector<unsigned char> compressed_color_buffer = {1, 2, 3, 4, 5};
static float *depth_buffer = nullptr;
static uint32_t width = 0;
static uint32_t height = 0;

// Not sure if its correct
#define GL_DEPTH_RANGE 0x0B70
#define GL_DEPTH_COMPONENT 0x1902

/// Globals
static uchar *camera = NULL;
static float x = 0, y = 0, z = 0,
             pitch = 0, yaw = 0, roll = 0;
static GLfloat near, far;

/// Functions
static void GameRenderer_moveCameraToPlayer_glRotatef_injection() {
    glDepthRangef(near, far);
    // glRotatef(pitch / 2.0, 1.0, 0.0, 0.0);
    // glRotatef(yaw,   0.0, 1.0, 0.0);
    glRotatef(roll, 0.0, 0.0, 1.0);
}

static void update_output_buffers(uint32_t new_width, uint32_t new_height) {
    if (new_width != width || new_height != height) {
        std::cout << "Rebuilding output buffers\n";
        std::cout << new_width << ":" << new_height << "\n";

        width = new_width;
        height = new_height;

        delete color_buffer;
        delete depth_buffer;

        color_buffer = new unsigned char[new_width * new_height * 4]; // RGBA color buffer
        depth_buffer = new float[new_width * new_height];
    }
}

std::string CommandServer_parse_injection(uchar *command_server, ConnectedClient &client, std::string const &command) {
    float x, y, z, pitch, yaw, roll;
    int ret = sscanf(command.c_str(), "mcpivr.set(%f,%f,%f,%f,%f,%f)\n", &x, &y, &z, &pitch, &yaw, &roll);
    if (ret == 6) {
        // Set
        ::x = x;
        ::y = y;
        ::z = z;
        ::pitch = pitch;
        ::yaw = yaw;
        ::roll = roll;
        return "";
    }

    ret = sscanf(command.c_str(), "mcpivr.getResolution(%[)]\n");
    if (ret == 1) {
        return std::to_string(width) + "," + std::to_string(height) + "\n";
    }

    ret = sscanf(command.c_str(), "mcpivr.render(%[)]\n");
    if (ret == 1) {
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, color_buffer);
        glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depth_buffer);
        return "";
    }

    GLfloat near, far;
    ret = sscanf(command.c_str(), "mcpivr.setClipPlanes(%f,%f)\n", &near, &far);
    if (ret == 2) {
        ::near = near;
        ::far = far;
        glDepthRangef(near, far);
        return "";
    }

    int page;
    ret = sscanf(command.c_str(), "mcpivr.getColor(%i)\n", &page);
    if (ret == 1) {
        int page_size = std::min((uint32_t)65536, (width * height * 4) - (page * 65536));
        // This may overflow on the last page, but im not sure
        // future me if you crash because of this exact reason, lmao
        // cast everything to int and it *should* work fine
        if (page_size <= 0) return "";

        std::string str(reinterpret_cast<char *>(color_buffer + (page * 65536)), page_size);
        return str;
    }

    ret = sscanf(command.c_str(), "mcpivr.getDepth(%i)\n", &page);
    if (ret == 1) {
        int page_size = std::min((uint32_t)65536, (width * height * 4) - (page * 65536));
        if (page_size <= 0) return "";

        std::string str(reinterpret_cast<char *>(depth_buffer + (page * 65536)), page_size);
        return str;
    }

    return (*CommandServer_parse)(command_server, client, command);
}

static inline void spawn_camera(uchar *minecraft) {
    uchar *level = *(uchar **)(minecraft + Minecraft_level_property_offset);
    camera = (uchar *)::operator new(CAMERA_ENTITY_SIZE);
    Mob(camera, level);
    *(uchar **)camera = CameraEntity_vtable;
    *(int *)(camera + CameraEntity_tracking_property_offset) = -1;
    (*Entity_moveTo)(camera, x, y, z, yaw, pitch);
    (*Level_addEntity)(level, camera);
    // Set size to zero
    *(float *)(camera + Entity_hitboxHeight_property_offset) = 0;
    *(float *)(camera + Entity_hitboxWidth_property_offset) = 0;
    // Adjust head hight
    *(float *)(camera + Entity_headHeight_property_offset) += 1.62;
    // Set camera to it
    *(uchar **)(minecraft + Minecraft_camera_property_offset) = camera;
}

static void mcpi_callback(uchar *minecraft) {
    if (!minecraft) return;
    uchar *level = *(uchar **)(minecraft + Minecraft_level_property_offset);
    uint32_t screen_width = *(uint32_t *)(minecraft + Minecraft_screen_width_property_offset);
    uint32_t screen_height = *(uint32_t *)(minecraft + Minecraft_screen_height_property_offset);

    if (screen_width && screen_height) {
        update_output_buffers(screen_width, screen_height);
        // glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, color_buffer);
        // compress_data();
        // std::string str(reinterpret_cast<const char*>(compressed_color_buffer.data()), compressed_color_buffer.size());
        // std::string str(reinterpret_cast<char*>(color_buffer), width * height * 4);
    }

    if (!level) {
        camera = NULL;
        return;
    }

    if (!camera) {
        // Spawn camera
        spawn_camera(minecraft);
    } else {
        *(uchar **)(minecraft + Minecraft_camera_property_offset) = camera;
        (*Entity_moveTo)(camera, x, y, z, yaw, pitch);
    }
}

/// Init
__attribute__((constructor)) static void init() {
    misc_run_on_update(mcpi_callback);
    // GameRenderer::moveCameraToPlayer starts at 0x482e0
    overwrite_call((void *)0x4835c, (void *)GameRenderer_moveCameraToPlayer_glRotatef_injection);
    // This is *not* compatable with MCPI-Addons
    overwrite_calls((void *)CommandServer_parse, (void *)CommandServer_parse_injection);
}
