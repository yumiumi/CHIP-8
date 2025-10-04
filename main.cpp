#include "raylib.h"
#include "raymath.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include "imgui.h"
#include "rlImGui.h"
#include "imgui_memory_editor.h"

using namespace std;

// RAM (first 512 bytes should not be used by programs)
uint8_t RAM[4096];

// 16 general purpose registers (8 bit)
uint8_t V[16];

// I register (16 bit)
uint16_t I;

// PC register (16 bit) - program counter
// stores the currently executing address
uint16_t PC;

// SP register (8 bit) - stack pointer
// points to the topmost level of the stack
uint8_t SP = 0x00;

// stores the address that interpreter shoud return to
// when finished with a subroutine
uint16_t stack[16];

// Delay timer register
// Delay timer subtr 1 from the val of DT at a rate of 60Hz
// When DT = 0, it deactivates
uint8_t DT;

// Sound timer register
uint8_t ST;

int const scr_h = 32;
int const scr_w = 64;
int cell_size = 8;
bool screen[scr_h][scr_w];

const float frames_per_sec = 60;

uint8_t sprite_table[16][5] = {
    {	0xF0, 0x90, 0x90, 0x90, 0xF0, }, // 0
    {	0x20, 0x60, 0x20, 0x20, 0x70, }, // 1
    {   0xF0, 0x10, 0xF0, 0x80, 0xF0, }, // 2
    {	0xF0, 0x10, 0xF0, 0x10, 0xF0, }, // 3
    {   0x90, 0x90, 0xF0, 0x10, 0x10, }, // 4
    {   0xF0, 0x80, 0xF0, 0x10, 0xF0, }, // 5
    {   0xF0, 0x80, 0xF0, 0x90, 0xF0, }, // 6
    {   0xF0, 0x10, 0x20, 0x40, 0x40, }, // 7
    {   0xF0, 0x90, 0xF0, 0x90, 0xF0, }, // 8
    {	0xF0, 0x90, 0xF0, 0x10, 0xF0, }, // 9
    {	0xF0, 0x90, 0xF0, 0x90, 0x90, }, // A
    {	0xE0, 0x90, 0xE0, 0x90, 0xE0, }, // B
    {	0xF0, 0x80, 0x80, 0x80, 0xF0, }, // C
    {   0xE0, 0x90, 0x90, 0x90, 0xE0, }, // D
    {	0xF0, 0x80, 0xF0, 0x80, 0xF0, }, // E
    {   0xF0, 0x80, 0xF0, 0x80, 0x80, }, // F
};

uint8_t program2[] = {
    // start is address 0x200 (512)
    0x60, 0x10, // put value 0x10 into V0                   (200) 1
    0x61, 0x16, // put value 0x16 into V1                   (202) 2

    0x22, 0x0A, // CALL, JUMP to 20A                        (204) 3
    0x70, 0x05, // V0 = V0 + 05 (0 + 5 = 5)  V0 = 27        (206) 12
    0x12, 0x12, // jump to 212                              (208) 13
    0x80, 0x13, // /XOR, V0 XOR V1; V0 = 6 (6)              (20A) 4
    0x22, 0x16, // CALL, JUMP to 216                        (20C) 5
    0x81, 0x06, // ret here, if lsb V1 = 1, then V1>>1 (0)  (20E) 10
    0x00, 0xEE, // RET                                      (210) 11

    0x00, 0xE0, // CLS                                      (212) 14
    0x12, 0x1E, // JUMP to 21E                              (214) 15
    0x80, 0x14, // ADD (v0 + v1) - jump here VF=0  V0=22    (216) 6
    0x63, 0x15, // put value 0x15 into V3                   (218) 7
    0x81, 0x35, // V1 - V3 (V1 = 1)	  V1 = 1        VF = 1	(21A) 8
    0x00, 0xEE, // RET                                      (21C) 9
};

uint8_t program3[] = {
    0x61, 0x01, // vx = nn, v1 = 20
    0x62, 0x02, // vx = nn, v1 = 20
    0x65, 0x05, // vx = nn, v1 = 20
    0x68, 0xFE, // vx = nn, v1 = 20

    0xF5, 0x1E, 

    //0xD8, 0x58, // Dxyn, D x=1 y=2, n = 8
    0xD2, 0x8C, // Dxyn, D x=1 y=2, n = 8

};

uint8_t keypad[16];

int ch8_to_rb_key(uint8_t ch8_key) {
    switch (ch8_key) {
    case (0x01):
        return KEY_ONE;

    case (0x02):
        return KEY_TWO;

    case (0x03):
        return KEY_THREE;

    case (0x0C):
        return KEY_FOUR;

    case (0x04):
        return KEY_Q;

    case (0x05):
        return KEY_W;

    case (0x06):
        return KEY_E;

    case (0x0D):
        return KEY_R;

    case (0x07):
        return KEY_A;

    case (0x08):
        return KEY_S;

    case (0x09):
        return KEY_D;

    case (0x0E):
        return KEY_F;

    case (0x0A):
        return KEY_Z;

    case (0x00):
        return KEY_X;

    case (0x0B):
        return KEY_C;

    case (0x0F):
        return KEY_V;

    default:
        break;
    }
}

uint8_t rb_to_ch8_key(int key) {
    switch (key) {
    case (KEY_ONE):
        return 0x01;

    case (KEY_TWO):
        return 0x02;

    case (KEY_THREE):
        return 0x03;

    case (KEY_FOUR):
        return 0x0C;

    case (KEY_Q):
        return 0x04;

    case (KEY_W):
        return 0x05;

    case (KEY_E):
        return 0x06;

    case (KEY_R):
        return 0x0D;

    case (KEY_A):
        return 0x07;

    case (KEY_S):
        return 0x08;

    case (KEY_D):
        return 0x09;

    case (KEY_F):
        return 0x0E;

    case (KEY_Z):
        return 0x0A;

    case (KEY_X):
        return 0x00;

    case (KEY_C):
        return 0x0B;

    case (KEY_V):
        return 0x0F;

    default:
        break;
    }
}

void load_to_memory() {
    for (int i = 0; i < sizeof(program2); i++) {
        RAM[512 + i] = program2[i];
    }
    // memcpy(chip8_RAM + 512, program, sizeof(program));
}

void load_sptites() {
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 5; x++) {
            if ((y * 5 + x) < 0x1FF) { // 0x1FF = 511
                // load sprites? start is RAM[0]
                RAM[y * 5 + x] = sprite_table[y][x];
            }
        }
    }
}

void init_disp() {
    for (int y = 0; y < scr_h; y++) {
        for (int x = 0; x < scr_w; x++) {
            screen[y][x] = 0;
        }
    }
}

void disp_clear() {
    for (int y = 0; y < scr_h; y++) {
        for (int x = 0; x < scr_w; x++) {
            screen[y][x] = 0;
        }
    }
    // memset(screen, 0, sizeof(screen));
}

// fetch, decode and execute a single instruction
void run_cycle() {

    bool was_changed = false;

    uint8_t xpos;
    uint8_t ypos;

    bool scr_end_w = false;
    bool scr_end_h = false;

    int key;

    // 4-bit value, the lowest 4 bit (Dxyn)
    uint8_t n = RAM[PC + 1] & 0x0F;

    // 4-bit value, the lower 4 bits of the high byte of instr (3xkk)
    uint8_t x = RAM[PC] & 0x0F;

    // 4-bit value, the lower 4 bits of the low byte o instr (8xy3)
    uint8_t y = (RAM[PC + 1] >> 4) & 0x0f;

    // 8-bit value, the lowest 8 bits of instr (3xkk)
    uint8_t kk = RAM[PC + 1];

    // 12-bit value, the lowest 12 bit of instr (0nnn)
    uint16_t nnn = (x << 8) | kk;

    // 4-bit value, the lower 4 bits of the high byte of instr, shifted right by 4
    uint8_t op_category = RAM[PC] >> 4;

    uint8_t rand_val;

    uint8_t sprite_row;
    uint8_t sprite_pixel;

    switch (op_category) {
    case (0x00):
        switch (n) {
        case(0x00):
            disp_clear();
            break;

        case(0x0E): // RET // good
            SP--;
            PC = stack[SP];
            break;
        }
        break;

    case (0x01): // Jump to nnn
        PC = nnn;
        PC -= 2;
        break;

    case (0x02): // CALL
        stack[SP] = PC;
        SP++;
        PC = nnn - 2; // later i add 2 to PC
        break;

    case (0x03):
        if (V[x] == kk) {
            PC += 2;
        }
        break;

    case (0x04):
        if (V[x] != kk) {
            PC += 2;
        }
        break;

    case (0x05):
        if (V[x] == V[y]) {
            PC += 2;
        }
        break;

    case (0x06):
        V[x] = kk;
        break;

    case (0x07):
        V[x] += kk;
        break;

    case(0x08):
        switch (n) {
            case (0x00):
                V[x] = V[y];
                break;

            case (0x01):
                V[x] = (V[x] | V[y]);
                break;

            case (0x02):
                V[x] = (V[x] & V[y]);
                break;

            case (0x03):
                V[x] = V[x] ^ V[y];
                break;

            case (0x04):
                V[x] = (V[x] + V[y]);
                if (V[x] > 0xFF) {
                    V[0x0F] = 1;
                }
                else {
                    V[0x0F] = 0;
                }
                V[x] &= 0xFF;
                break;

            case (0x05):
                if (V[x] > V[y]) {
                    V[0x0F] = 1;
                }
                else {
                    V[0x0F] = 0;
                }
                V[x] -= V[y];
                break;

            case (0x06):
                V[0x0F] = V[x] & 0x01;
                V[x] >>= 1;
                break;

            case (0x07):
                if (V[y] >= V[x]) {
                    V[0x0F] = 1;
                }
                else {
                    V[0x0F] = 0;
                }
                V[x] = V[y] - V[x];
                break;

            case (0x0E): // good 
                if ((V[x] & 0x80) == 0x80) {
                    V[0x0F] = 1;
                }
                else {
                    V[0x0F] = 0;
                }
                V[x] = V[x] * 2;
                break;
        }
        break;

    case (0x09):
        if (V[x] != V[y]) {
            PC += 2;
        }
        break;

    case (0x0A): // good
        I = nnn;
        break;

    case (0x0B):
        PC = nnn + V[0];
        break;

    case (0x0C):
        rand_val = GetRandomValue(0, 255);
        V[x] = rand_val & kk;
        break;

    case (0x0D):
        V[0x0F] = 0;
        was_changed = false;
        for (int h = 0; h < n; h++) {
            sprite_row = RAM[I + h];
            for (int w = 0; w < 8; w++) {
                sprite_pixel = sprite_row & (0x80 >> w);
                ypos = (V[y] + h) % scr_h;
                xpos = (V[x] + w) % scr_w;
                if (xpos == (scr_w - 1)) {
                    scr_end_w = true;
                }
                if (ypos == (scr_h - 1)) {
                    scr_end_h = true;
                }
                if (sprite_pixel != 0) {
                    if (screen[ypos][xpos] == 1) {
                        screen[ypos][xpos] = 0;
                        was_changed = true;
                    }
                    else {
                        screen[ypos][xpos] = 1;
                    }
                }
                if (scr_end_w) {
                    scr_end_w = false;
                    break;
                }
            }
            if (scr_end_h) {
                scr_end_h = false;
                break;
            }
        }
        if (was_changed) {
            V[0x0F] = 1;
        }
        break;

    case (0x0E):
        switch (kk) {
        case (0x9E):
            key = ch8_to_rb_key(V[x]);
            if (IsKeyDown(ch8_to_rb_key(V[x]))) {
                key = ch8_to_rb_key(V[x]);
                PC += 2;
            }
            break;

        case (0xA1):
            key = ch8_to_rb_key(V[x]);
            if (!IsKeyDown(ch8_to_rb_key(V[x]))) {
                key = ch8_to_rb_key(V[x]);
                PC += 2;
            }
            break;
        }
        break;

    case (0x0F):
        switch (kk) {
            case (0x07):
                V[x] = DT;
                break;

            case (0x0A): // check this later
                if (IsKeyReleased(KEY_ONE)) {
                    V[x] = rb_to_ch8_key(KEY_ONE);
                }
                else if (IsKeyReleased(KEY_TWO)) {
                    V[x] = rb_to_ch8_key(KEY_TWO);
                }
                else if (IsKeyReleased(KEY_THREE)) {
                    V[x] = rb_to_ch8_key(KEY_THREE);
                }
                else if (IsKeyReleased(KEY_FOUR)) {
                    V[x] = rb_to_ch8_key(KEY_FOUR);
                }
                else if (IsKeyReleased(KEY_Q)) {
                    V[x] = rb_to_ch8_key(KEY_Q);
                }
                else if (IsKeyReleased(KEY_W)) {
                    V[x] = rb_to_ch8_key(KEY_W);
                }
                else if (IsKeyReleased(KEY_E)) {
                    V[x] = rb_to_ch8_key(KEY_E);
                }
                else if (IsKeyReleased(KEY_R)) {
                    V[x] = rb_to_ch8_key(KEY_R);
                }
                else if (IsKeyReleased(KEY_A)) {
                    V[x] = rb_to_ch8_key(KEY_A);
                }
                else if (IsKeyReleased(KEY_S)) {
                    V[x] = rb_to_ch8_key(KEY_S);
                }
                else if (IsKeyReleased(KEY_D)) {
                    V[x] = rb_to_ch8_key(KEY_D);
                }
                else if (IsKeyReleased(KEY_F)) {
                    V[x] = rb_to_ch8_key(KEY_F);
                }
                else if (IsKeyReleased(KEY_Z)) {
                    V[x] = rb_to_ch8_key(KEY_Z);
                }
                else if (IsKeyReleased(KEY_X)) {
                    V[x] = rb_to_ch8_key(KEY_X);
                }
                else if (IsKeyReleased(KEY_C)) {
                    V[x] = rb_to_ch8_key(KEY_C);
                }
                else if (IsKeyReleased(KEY_V)) {
                    V[x] = rb_to_ch8_key(KEY_V);
                }
                else {
                    PC -= 2;
                }
                break;

            case (0x15):
                DT = V[x];
                break;

            case (0x18):
                ST = V[x];
                break;

            case (0x1E):
                I = I + V[x];
                break;

            case (0x29):
                I = V[x] * 5; // not sure
                break;

            case (0x33):
                if (V[x] == 0) {
                    RAM[I] = 0;
                    RAM[I + 1] = 0;
                    RAM[I + 2] = 0;
                }
                else {
                    RAM[I] = V[x] / 100;
                    RAM[I + 1] = (V[x] / 10) % 10;
                    RAM[I + 2] = V[x] % 10;
                }
                break;

            case (0x55):
                for (int i = 0; i <= x; i++) {
                    RAM[I + i] = V[i];
                }
                break;

            case (0x65):
                for (int i = 0; i <= x; i++) {
                    V[i] = RAM[I + i];
                }
                break;
        }
        break;

    default:
        break;
    }
}

Vector2 convert_to_px(Vector2 v) {
    Vector2 v_px = { v.x * cell_size, v.y * cell_size };
    //centerize
    float w = 1920/2 - scr_w/2 * cell_size;
    float h = 1080/2 - scr_h/2 * cell_size;
    return { v_px.x + w , v_px.y + h };
}

void render_screen() {
    Vector2 v2cell_sz = { float(cell_size), float(cell_size) };
    for (int y = 0; y < scr_h; y++) {
        for (int x = 0; x < scr_w; x++) {
            Vector2 x_y = { float(x), float(y) };
            if (screen[y][x] == 0) {
                DrawRectangleV(convert_to_px(x_y), v2cell_sz, RAYWHITE);
            }
            if (screen[y][x] == 1) {
                DrawRectangleV(convert_to_px(x_y), v2cell_sz, BLACK);
            }
        }
    }
}

int main() {
    InitWindow(1920, 1080, "CHIP-8");
    init_disp();
    load_sptites();
    
    //open the binary file for reading
    //FILE *program_file = fopen("glitchGhost.ch8", "rb");
    //FILE *program_file = fopen("3-corax+(1).ch8", "rb");
    FILE *program_file = fopen("5-quirks(1).ch8", "rb");
    //FILE *program_file = fopen("4-flags.ch8", "rb");

    if (program_file) {
        fseek(program_file, 0, SEEK_END);
        long program_size = ftell(program_file);
        fseek(program_file, 0, SEEK_SET);

        fread(RAM + 512, program_size, 1, program_file);
        fclose(program_file);
    }
    else {
        cout << "ERROR: opeing file for reading " << endl;
    }

    //memcpy(RAM + 512, program3, sizeof(program3));
    PC = 0x200; // start at 512
    double next_tick = 0.f;

    SetTargetFPS(60);
    rlImGuiSetup(true);

    while (!WindowShouldClose()) {
        BeginDrawing();
            ClearBackground(BLACK);

            // start ImGui
            rlImGuiBegin();
            bool open = true;
            ImGui::ShowDemoWindow(&open);
            open = true;
            if (ImGui::Begin("Test Window", &open))
            {
                ImGui::Text("PC: 0x%x\n", PC );
                ImGui::Text("I: 0x%x\n", I);
                ImGui::Text("SP: 0x%02x\n", SP);
                ImGui::Text("DT: 0x%02x\n", DT);
                ImGui::Text("ST: 0x%02x\n", ST);

                    for (int i = 0; i < sizeof(V); i++) {
                        ImGui::Text("V%x: 0x%02x\n", i , V[i]);
                    }

                static MemoryEditor mem_edit;
                mem_edit.OptShowAscii = false;
                mem_edit.Cols = 8;
                mem_edit.HighlightColor = IM_COL32(255, 64, 64, 128);
                mem_edit.HighlightMin = PC - 0x200;
                mem_edit.HighlightMax = PC + 2 - 0x200;
                mem_edit.DrawWindow(
                    "Memory Editor",
                    RAM + 0x200,
                    4096 - 0x200,
                    0x200
                );
            }
            ImGui::End();
            rlImGuiEnd();

            if (GetTime() >= next_tick) {
                next_tick = GetTime() + (1.f / 60.0f);
                
                for (int i = 0; i < 9; i++) {
                    run_cycle();
                    PC += 2;
                }
                if (DT > 0) {
                    DT -= 1;
                }
                if (ST > 0) {
                    ST -= 1;
                }
            }

            /*if (IsKeyPressed(KEY_SPACE)) {
                 run_cycle();
                PC += 2;
                if (DT > 0) {
                    DT -= 1;
                }
                if (ST > 0) {
                    ST -= 1;
                }
            }*/

            render_screen();

        EndDrawing();
    }
    CloseWindow();
    return 0;
}
