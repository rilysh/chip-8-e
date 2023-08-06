#ifndef CHIP8_CODE_H
#define CHIP8_CODE_H       1

#include <SDL2/SDL.h>

#define RED            "\033[0;91m"
#define WHITE          "\033[0;37m"
#define END            "\033[0m"

#define V_SIZE         16
#define KEY_SIZE       16
#define STACK_SIZE     (16 - 1)
#define MEMORY_SIZE    4096
#define GFX_SIZE       (64 * 32)
#define PIXELS_SIZE    2048
#define x(opcode)      ((opcode & 0xF00) >> 8)
#define y(opcode)      ((opcode & 0xF0) >> 4)
#define n(opcode)      (opcode & 0xF)
#define F              0xF
#define kk(opcode)     (opcode & 0xFF)

/* Compiler macro to ignore unused variable */
#define UNUSED        __attribute__((unused))

struct sdl_system {
    SDL_Event event;
    SDL_Window *window;
    SDL_Renderer *render;
    SDL_Texture *texture;
};

struct audio_system {
    unsigned int wav_length;
    unsigned char *wav_buffer;
    SDL_AudioSpec wav_spec;
    SDL_AudioDeviceID audio_dev_id;
};

struct option_config {
    int fi_opt;
    int fc_opt;
    int sc_opt;
    int fa_opt;
    int cd_opt;
    int ww_opt;
    int wh_opt;
    int fr_opt;
};

struct option_args {
    unsigned int fore_color;
    unsigned int back_color;
    unsigned int frame_after;
    unsigned int copy_delay;
    unsigned int window_width;
    unsigned int window_height;
    char *file_name;
};

#endif
