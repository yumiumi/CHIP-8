#include "raylib.h"
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
uint8_t SP;

// stores the address that interpreter shoud return to
// when finished with a subroutine
uint16_t stack[16];


int const scr_h = 32;
int const scr_w = 64;
int cell_size = 8;
bool screen[scr_h][scr_w];

uint8_t program[] = {
    // start is address 0x200 (512), 0x201
    0x60, 0x10, // put value 0x10 (16) into V0 // (6x kk, Vx = kk)
    // 0x202, 0x203
    0x61, 0x01, // put value 0x01 (1) into V1
    // 0x204, 0x205
    0x80, 0x15, // V0 = V0 - V1
	// 0x206, 0x207
    0x30, 0x00, // skip next instruction if V0 == 0
	// 0x208, 0x209
    0x12, 0x04, // jump to 0x204
};

void load_to_memory() {
	for (int i = 0; i < sizeof(program); i++) {
		RAM[512 + i] = program[i];
	}
	// memcpy(chip8_RAM + 512, program, sizeof(program));
}

void execute_instruction() {
	// 4-bit value, the lowest 4 bit (Dxyn)
	uint8_t n;

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
	case (0x01):
		PC = nnn;
		break;

	case (0x03):
		if (V[x] == kk) {
			PC += 2;
		}
		PC += 2;
		break;

	case (0x06):
		V[x] = kk;
		PC += 2;
		break;

	case(0x08):
		switch (kk & 0x0F) {
		case (0x05):
			V[x] -= V[y];
			uint8_t test = V[x];
			PC += 2;
			int i = 0;
			break;
		}

	default:
		break;
	}
}

void init_screen() {
	for (int y = 0; y < scr_h; y++) {
		for (int x = 0; x < scr_w; x++) {
			screen[y][x] = 0;
		}
	}
	// memset(screen, 0, sizeof(screen));
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
	init_screen();
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

				//ImGui::Text("r_half: 0x%02x\n", chip8_RAM[PC] >> 4);

				//if (ImGui::CollapsingHeader("V(0-f)", ImGuiTreeNodeFlags_None))
				//{
					for (int i = 0; i < sizeof(V); i++) {
						ImGui::Text("V%x: 0x%02x\n", i , V[i]);
					}
				//}

				static MemoryEditor mem_edit;
				mem_edit.DrawWindow("Memory Editor", RAM, 4096);

			}
			ImGui::End();
			rlImGuiEnd();

			if (IsKeyPressed(KEY_SPACE)) {
				execute_instruction();
			}

			render_screen();

		EndDrawing();
	}
	CloseWindow();
	return 0;
}
