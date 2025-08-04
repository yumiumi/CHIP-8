﻿#include "raylib.h"
#include "raymath.h"
#include <iostream>
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

int const scr_h = 32;
int const scr_w = 64;
int cell_size = 8;
bool screen[scr_h][scr_w];

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
    for (int y = 10; y < 22; y++) {
        for (int x = 8; x < 15; x++) {
            screen[y][x] = 1;
        }
    }
    screen[27][33] = 1;
}

void disp_clear() {
    for (int y = 0; y < scr_h; y++) {
        for (int x = 0; x < scr_w; x++) {
            screen[y][x] = 0;
        }
    }
    // memset(screen, 0, sizeof(screen));
}

void execute_instruction() {
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

    switch (op_category) {
    case (0x00):
        switch (n) {
        case(0x00):
            disp_clear();
            break;

        case(0x0E): // RET
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
        PC = nnn;
        PC -= 2;
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
                V[x] = (V[x] + V[y]); // Only the lowest 8 bits of the result are kept, and stored in Vx
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
                if ((V[x] & 0x0F) == 0x01) {
                    V[0x0F] = 1; 
                }
                else {
                    V[0x0F] = 0; 
                }
                V[x] = V[x] >> 1;
                break;

            case (0x07):
                if (V[y] > V[x]) {
                    V[0x0F] = 1;
                }
                else {
                    V[0x0F] = 0;
                }
                V[x] = V[y] - V[x];
                break;

            case (0x0E):
                if ((V[x] & 0xF0) == 0x10) {
                    V[0x0F] = 1;
                }
                else {
                    V[0x0F] = 0;
                }
                V[x] = V[x] << 2;
                break;
        }
    break;

    case (0x09):
        if (V[x] != V[y]) {
            PC += 2;
        }
        break;

    case (0x0A):
        I = nnn;
        break;

    case (0x0B):
        PC = nnn + V[0];
        break;

    /*case (0x0C): // ----------- this
        // ...
        break;

    case (0x0D): // ----------- this
        // ...
        break;*/

    // case Ex9E and ExA1
    // case F...

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
    Vector2 v2cell_sz = { cell_size, cell_size };
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
    load_to_memory();
    PC = 0x200; // start at 512

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

            if (IsKeyPressed(KEY_SPACE)) {
                execute_instruction();
                PC += 2;
            }

            render_screen();

        EndDrawing();
    }
    CloseWindow();
    return 0;
}
