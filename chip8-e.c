#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <pthread.h>
#include <SDL2/SDL.h>

#include "chip8-e.h"

/* structs */
static struct sdl_system sdl_sys;
static struct audio_system asys;

/* variables */
static unsigned short opcode, pc, idx, nnn, stack[STACK_SIZE + 1];
static unsigned char dt, st, sp, draw, V[V_SIZE], memory[MEMORY_SIZE], keys[KEY_SIZE], gfx[GFX_SIZE];

/* chip-8 fontset */
static unsigned char chip8_fonts[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0,     /* 0 */
    0x20, 0x60, 0x20, 0x20, 0x70,     /* 1 */
    0xF0, 0x10, 0xF0, 0x80, 0xF0,     /* 2 */
    0xF0, 0x10, 0xF0, 0x10, 0xF0,     /* 3 */
    0x90, 0x90, 0xF0, 0x10, 0x10,     /* 4 */
    0xF0, 0x80, 0xF0, 0x10, 0xF0,     /* 5 */
    0xF0, 0x80, 0xF0, 0x90, 0xF0,     /* 6 */
    0xF0, 0x10, 0x20, 0x40, 0x40,     /* 7 */
    0xF0, 0x90, 0xF0, 0x90, 0xF0,     /* 8 */
    0xF0, 0x90, 0xF0, 0x10, 0xF0,     /* 9 */
    0xF0, 0x90, 0xF0, 0x90, 0x90,     /* A */
    0xE0, 0x90, 0xE0, 0x90, 0xE0,     /* B */
    0xF0, 0x80, 0x80, 0x80, 0xF0,     /* C */
    0xE0, 0x90, 0x90, 0x90, 0xE0,     /* D */
    0xF0, 0x80, 0xF0, 0x80, 0xF0,     /* E */
    0xF0, 0x80, 0xF0, 0x80, 0x80      /* F */
};

/* chip-8 keypad */
static char keypad[] = {
    0x31, 0x32, 0x33, 0x34,
    0x71, 0x77, 0x65, 0x72,
    0x61, 0x73, 0x64, 0x66,
    0x7a, 0x78, 0x63, 0x76
};

/* debug_log: enable/disable showing instruction logs */
static __inline__ void debug_log(unsigned short op, const char *msg)
{
    #ifdef DEBUG_LOG
        fprintf(stdout,
		"%sopcode: %s%#06x, %sinstruction: %s%s%s\n",
		RED, WHITE, op, RED, WHITE, msg, END
	    );
    #endif
}

/* ferr: wrapper like of fprintf(stderr, ...) */
static void ferr(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}

/* strtint: convert string to integer  */
static unsigned int strtint(const char *value)
{
    long val;
    char *eptr;

    val = strtol(value, &eptr, 0);

    if (eptr == value)
	ferr("Error: Provided value isn't a number\n");

    return (unsigned int)val;
}

/* xorshift32: PRNG function */
static unsigned int xorshift32(unsigned int seed)
{
    unsigned int x;

    x = seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    seed = x;

    return seed;
}

/* init_sdl2: initialize SDL2 and all used subsystems */
static void init_sdl2(void)
{
    unsigned int flags;

    flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS;

    if (SDL_Init(flags) != 0)
	ferr("SDL initalization failed, error: %s\n", SDL_GetError());
}

/* init_audio_subsystem: initialize audio sytem */
static void init_audio_subsystem(void)
{
    if (SDL_LoadWAV("audio/beep.wav", &asys.wav_spec, &asys.wav_buffer, &asys.wav_length) == NULL)
	ferr("SDL_LoadWAV() failed, error: %s\n", SDL_GetError());

    asys.audio_dev_id = SDL_OpenAudioDevice(NULL, 0, &asys.wav_spec, NULL, 0);
    if (asys.audio_dev_id == 0)
	ferr("SDL_OpenAudioDevice() failed, error: %s\n", SDL_GetError());
}

/* audio_play_beep: play beep audio through SDL */
static void *audio_play_beep(UNUSED void *unused_argument)
{
    if (SDL_QueueAudio(asys.audio_dev_id, (unsigned char *)asys.wav_buffer, asys.wav_length) != 0)
	ferr("SDL_QueueAudio() failed, error: %s\n", SDL_GetError());

    SDL_PauseAudioDevice(asys.audio_dev_id, 0);
    SDL_Delay(80);

    return NULL;
}

/* init_regs: initialize variables */
static void init_regs(void)
{
    int i;

    /* Assign program counter value to the start of the CHIP-8 program, and set others to zero */ 
    pc = 0x200;
    opcode = idx = sp = nnn = dt = 0;

    /* Clear out stack, memory, gfx, and V */
    memset(stack, '\0', STACK_SIZE);
    memset(memory, '\0', MEMORY_SIZE);
    memset(gfx, '\0', GFX_SIZE);
    memset(V, '\0', V_SIZE);

    /* Set CHIP-8 fonts to the memory */
    for (i = 0; i < 80; i++)
	memory[i] = chip8_fonts[i];
}

/* load_rom: open the ROM and load into memory */
static void load_rom(const char *file)
{
    long sz;
    FILE *fp;

    fp = fopen(file, "rb");
    if (fp == NULL) {
        perror("fopen()");
        exit(EXIT_FAILURE);
    }

    fseek(fp, 1L, SEEK_END);
    sz = ftell(fp);
    rewind(fp);

    if (sz <= 0 || sz > 4096) {
	fclose(fp);
	ferr("Error: The ROM size is invalid. ROM size must be > 0 and < 4096\n");
    }

    fread(memory + 512, 1UL, (size_t)sz, fp);
    fclose(fp);
}

/* fde_cycle: main emulation function, contains 36 cases */
static void fde_cycle(void)
{
    pthread_t tid;
    pthread_attr_t attr, *attrp;
    unsigned short xline, yline, xd, yd, hd, pix, i;

    opcode = (unsigned short)((memory[pc] << 8) | memory[pc + 1]);
    nnn = opcode & 0xFFF;
    pc += 2;

    switch (opcode & 0xF000) {
	/* SYS addr */
    case 0x0000:
	debug_log(opcode, "SYS addr");
	if ((opcode) == 0xE0) {
	    memset(gfx, '\0', 2048);
	    draw = 1;
	} else if ((opcode) == 0xEE) {
	    --sp;
	    pc = stack[sp];
	}
	break;

	/* JP addr */
    case 0x1000:
	debug_log(opcode, "JP addr");
	pc = nnn;
	break;

	/* CALL addr */
    case 0x2000:
	debug_log(opcode, "CALL addr");
	stack[sp] = pc;
	++sp;
	pc = nnn;
	break;

	/* SE Vx, byte */
    case 0x3000:
	debug_log(opcode, "SE Vx, byte");
	if (V[x(opcode)] == kk(opcode))
	    pc += 2;
	break;

	/* SNE Vx, byte */
    case 0x4000:
	debug_log(opcode, "SNE Vx, byte");
	if (V[x(opcode)] != kk(opcode))
	    pc +=2;
	break;

	/* SE Vx, Vy */
    case 0x5000:
	debug_log(opcode, "SE Vx, Vy");
	if (V[x(opcode)] == V[y(opcode)])
	    pc += 2;
	break;

	/* LD Vx, byte */
    case 0x6000:
	debug_log(opcode, "LD Vx, byte");
	V[x(opcode)] = kk(opcode);
	break;

	/* ADD Vx, byte */
    case 0x7000:
	debug_log(opcode, "ADD Vx, byte");
	V[x(opcode)] += kk(opcode);
	break;

	/* Children of 0x8000 */
    case 0x8000:
	debug_log(opcode, "Children of 0x8000");
	switch (opcode & 0xF) {
	    /* LD Vx, Vy */
	case 0x0:
	    debug_log(opcode, "LD Vx, Vy");
	    V[x(opcode)] = V[y(opcode)];
	    break;

	    /* OR Vx, Vy */
	case 0x1:
	    debug_log(opcode, "OR Vx, Vy");
	    V[x(opcode)] |= V[y(opcode)];
	    break;

	    /* AND Vx, Vy */
	case 0x2:
	    debug_log(opcode, "AND Vx, Vy");
	    V[x(opcode)] &= V[y(opcode)];
	    break;

	    /* XOR Vx, Vy */
	case 0x3:
	    debug_log(opcode, "XOR Vx, Vy");
	    V[x(opcode)] ^= V[y(opcode)];
	    break;

	    /* ADD Vx, Vy */
	case 0x4:
	    debug_log(opcode, "ADD Vx, Vy");
	    if ((V[x(opcode)] + V[y(opcode)]) > 255)
		V[F] = 1;
	    else
		V[F] = 0;

	    V[x(opcode)] += V[y(opcode)];
	    break;

	    /* SUB Vx, Vy */
	case 0x5:
	    debug_log(opcode, "SUB Vx, Vy");
	    if (V[x(opcode)] > V[y(opcode)])
		V[F] = 1;
	    else
		V[F] = 0;

	    V[x(opcode)] -= V[y(opcode)];
	    break;

	    /* SHR Vx {, Vy} */
	case 0x6:
	    debug_log(opcode, "SHR Vx {, Vy}");
	    V[F] = V[x(opcode)] & 1;
	    V[x(opcode)] >>= 1;
	    break;

	    /* SUBN Vx, Vy */
	case 0x7:
	    debug_log(opcode, "SUBN Vx, Vy");
	    if (V[y(opcode)] > V[x(opcode)])
		V[F] = 1;
	    else
		V[F] = 0;

	    V[x(opcode)] = V[y(opcode)] - V[x(opcode)];
	    break;

	    /* SHL Vx {, Vy} */
	case 0xE:
	    debug_log(opcode, "SHL Vx {, Vy}");
	    V[F] = V[x(opcode)] >> 7;
	    V[x(opcode)] <<= 1;
	    break;

	default:
	    debug_log(opcode, "unimplemented\n");
	    abort();
	    /* unreachable */
	}
	break;

	/* SNE Vx, Vy */
    case 0x9000:
	debug_log(opcode, "SNE Vx, Vy");
	if (V[x(opcode)] != V[y(opcode)])
	    pc += 2;
	break;

	/* LD I, addr */
    case 0xA000:
	debug_log(opcode, "LD I, addr"); 
	idx = nnn;
	break;

	/* JP V0, addr */
    case 0xB000:
	debug_log(opcode, "JP V0, addr");
	pc = (nnn) + V[0];
	break;

	/* RND Vx, byte */
    case 0xC000:
	debug_log(opcode, "RND Vx, byte");
	V[x(opcode)] = (xorshift32((unsigned int)time(NULL)) % 256) & kk(opcode);
	break;

	/* DRW Vx, Vy, nibble */
    case 0xD000:
	debug_log(opcode, "DRW Vx, Vy, nibble");
	xd = V[x(opcode)];
	yd = V[y(opcode)];
	hd = n(opcode);
	V[F] = 0;

	for (yline = 0; yline < hd; yline++) {
	    pix = memory[idx + yline];
	    for (xline = 0; xline < 8; xline++) {
		if ((pix & (0x80 >> xline)) != 0) {
		    if (gfx[(xd + xline + ((yd + yline) * 64))] == 1)
			V[F] = 1;

		    gfx[xd + xline + ((yd + yline) * 64)] ^= 1;
		}
	    }
	}

	draw = 1;
	break;

	/* Children of 0xE000 */
    case 0xE000:
	/* SKP Vx */
	if (kk(opcode) == 0x9E) {
	    debug_log(opcode, "SKP Vx");
	    if (keys[V[x(opcode)]] != 0) {
		pc += 2;
	    }
	} /*  SKNP Vx */
	else if (kk(opcode) == 0xA1) {
	    debug_log(opcode, "SKNP Vx");
	    if (keys[V[x(opcode)]] == 0) {
		pc += 2;
	    }
	}
	break;

	/* Children of 0xF000 */
    case 0xF000:
	switch (kk(opcode)) {
	    /* LD Vx, DT */
	case 0x7:
	    debug_log(opcode, "LD Vx, DT");
	    V[x(opcode)] = dt;
	    break;

	    /* LD Vx, K */
	case 0xA:
	    debug_log(opcode, "LD Vx, K");
	    for (i = 0; i < 16; i++)
                if (keys[i] != 0)
                    V[x(opcode)] = (unsigned char)i;
	    break;

	    /* LD DT, Vx */
	case 0x15:
	    debug_log(opcode, "LD DT, Vx");
	    dt = V[x(opcode)];
	    break;

	    /* LD ST, Vx */
	case 0x18:
	    debug_log(opcode, "LD ST, Vx");
	    st = V[x(opcode)];
	    break;

	    /* ADD I, Vx */
	case 0x1E:
	    debug_log(opcode, "ADD I, Vx");
	    idx += V[x(opcode)];
	    break;

	    /* LD F, Vx */
	case 0x29:
	    debug_log(opcode, "LD F, Vx");
	    idx = V[x(opcode)] * 5;
	    break;

	    /* LD B, Vx */
	case 0x33:
	    debug_log(opcode, "LD B, Vx");
	    memory[idx] = V[x(opcode)] / 100;
	    memory[idx + 1] = (V[x(opcode)] / 10) % 10;
	    memory[idx + 2] = V[x(opcode)] % 10;
	    break;

	    /* LD [I], Vx */
	case 0x55:
	    debug_log(opcode, "LD [I], Vx");
	    for (i = 0; i <= x(opcode); i++)
		memory[idx + i] = V[i];
            break;

	    /* LD Vx, [I] */
	case 0x65:
	    debug_log(opcode, "LD Vx, [I]"); 
	    for (i = 0; i <= x(opcode); i++)
		V[i] = memory[idx + i];
	    break;

	default:
	    debug_log(opcode, "unimplemented\n");
	    abort();
	    /* unreachable */
	}
	break;

    default:
	debug_log(opcode, "unimplemented\n");
	abort();
	/* unreachable */
    }

    if (dt > 0)
	dt--;

    if (st > 0) {
	st--;
	if (st == 0) {
	    attrp = &attr;
	    pthread_attr_init(&attr);
	    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	    if (pthread_create(&tid, attrp, audio_play_beep, NULL) != 0)
		ferr("pthread_create() failed\n");
	    if (pthread_attr_destroy(attrp) != 0)
		ferr("pthread_attr_destroy() failed");
	}
    }
}

/* clean_memory: free SDL allocated memory when done */
static void clean_memory(void)
{
    /* Cleanup audio subsystem */
    SDL_CloseAudioDevice(asys.audio_dev_id);
    SDL_FreeWAV(asys.wav_buffer);

    /* Cleanup SDL2 system */
    SDL_DestroyRenderer(sdl_sys.render);
    SDL_DestroyWindow(sdl_sys.window);
    SDL_Quit();
}

/* usage: show options and arguments*/
static void usage(void)
{
    fprintf(
	stdout,
	"chip8-e - A simple CHIP-8 emulator\n"
	"Usage:\n"
	"  --file [file_name]      -- The ROM file name to emulate\n"
	"  --fore-color [color]    -- Window foreground color\n"
	"  --back-color [color]    -- Window background color\n"
	"  --frame-after [time]    -- How many frame after SDL should render\n"
	"  --copy-delay [time[     -- Delay between the last and the next copy\n"
	"  --window-width [size]   -- Set SDL window width\n"
	"  --window-height [size]  -- Set SDL window height\n\n" 
	);
}

int main(int argc, char **argv)
{
    int opt, fallback = 0;
    unsigned int pixels[PIXELS_SIZE], i, passes, running;

    if (argc < 2 || argv[1][0] != '-') {
	usage();
	exit(EXIT_FAILURE);
    }

    /* op_args: passed arguments value will be store here */
    struct option_args op_args = {
	.copy_delay = 5, .fore_color = 0xFFFFFF, .frame_after = 1,
	.back_color = 0xFF000000, .window_height = 500, .window_width = 900,
	.file_name = NULL
    };

    /* options: arguments flags */
    struct option options[] = {
	{ "file",            required_argument, 0, 1 },
	{ "fore-color",      required_argument, 0, 2 },
	{ "back-color",      required_argument, 0, 3 },
	{ "frame-after",     required_argument, 0, 4 },
	{ "copy-delay",      required_argument, 0, 5 },
	{ "window-width",    required_argument, 0, 6 },
	{ "window-height",   required_argument, 0, 7 },
	{ "fallback-render", no_argument,       0, 8 },
	{ "help",            no_argument,       0, 9 }
    };

    while ((opt = getopt_long(argc, argv, "", options, NULL)) != -1) {
	switch (opt) {
	    /* --file argument */
	case 1:
	    op_args.file_name = optarg;
	    break;

	    /* --fore-color argument */
	case 2:
	    op_args.fore_color = strtint(optarg);
	    break;

	    /* --back-color argument */
	case 3:
	    op_args.back_color = strtint(optarg);
	    break;

	    /* --frame-after argument */
	case 4:
	    op_args.frame_after = strtint(optarg);
	    break;

	    /* --copy-delay argument */
	case 5:
	    op_args.copy_delay = strtint(optarg);
	    break;

	    /* --window-width argument */
	case 6:
	    op_args.window_width = strtint(optarg);
	    break;

	    /* --window-height argument */
	case 7:
	    op_args.window_height = strtint(optarg);
	    break;

	    /* --fallback-render argument */
	case 8:
	    fallback = 1;
	    break;

	    /* --help argument */
	case 9:
	    usage();
	    exit(EXIT_SUCCESS);

	    /* default (unknown argument) */
	default:
	    fprintf(stdout, "hit default\n");
	    break;
	}
    }

    /* check --file argument */
    if (op_args.file_name == NULL)
	ferr("Error: No ROM file was specified\n");

    init_sdl2();
    init_audio_subsystem();
    memset(pixels, '\0', PIXELS_SIZE - 1);

    sdl_sys.window = SDL_CreateWindow(
        "Chip-8-e", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        (int)op_args.window_width, (int)op_args.window_height, 0
    );
    if (sdl_sys.window == NULL) {
        fprintf(stderr, "SDL_CreateWindow() failed, error: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    /* if fallback is needed */
    if (fallback)
	sdl_sys.render = SDL_CreateRenderer(sdl_sys.window, -1, SDL_RENDERER_SOFTWARE);
    else
	sdl_sys.render = SDL_CreateRenderer(sdl_sys.window, -1, SDL_RENDERER_ACCELERATED);

    if (sdl_sys.render == NULL) {
	fprintf(stderr, "SDL_CreateRender() failed, error: %s\n", SDL_GetError());
	exit(EXIT_FAILURE);
    }

    sdl_sys.texture = SDL_CreateTexture(sdl_sys.render, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);

    passes = 0;
    running = 1;
    init_regs();
    load_rom(op_args.file_name);

    while (running) {
        fde_cycle();

	while (SDL_PollEvent(&sdl_sys.event)) {
	    switch (sdl_sys.event.type) {
	    case SDL_QUIT:
	        clean_memory();
		running = 0;
		exit(EXIT_SUCCESS);

	    case SDL_KEYDOWN:
		for (i = 0; i < 16; i++)
		    if (sdl_sys.event.key.keysym.sym == keypad[i])
			keys[i] = 1; /* TRUE */
		break;

	    case SDL_KEYUP:
		for (i = 0; i < 16; i++)
		    if (sdl_sys.event.key.keysym.sym == keypad[i])
			keys[i] = 0; /* FALSE */
		break;

	    case SDL_WINDOWEVENT:
		if (sdl_sys.event.window.event == SDL_WINDOWEVENT_CLOSE) {
		    clean_memory();
		    running = 0;
		    exit(EXIT_SUCCESS);
		}
		break;
	    }
	}

	if (draw) {
	    for (i = 0; i < 2048; i++)
		pixels[i] = (op_args.fore_color * gfx[i]) | op_args.back_color;
	    SDL_UpdateTexture(sdl_sys.texture, NULL, pixels, 64 * 4);
	    SDL_RenderCopy(sdl_sys.render, sdl_sys.texture, NULL, NULL);
	}

	if (passes == op_args.frame_after) {
	    SDL_RenderPresent(sdl_sys.render);
	    SDL_Delay(op_args.copy_delay);
	    passes = 0;
	} else {
	    passes++;
	}
    }
}
