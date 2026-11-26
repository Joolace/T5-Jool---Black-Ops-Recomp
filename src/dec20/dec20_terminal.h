#pragma once

struct Font_s;
struct rectDef_s;

// The frontend's DEC-20 computer terminal (SP). The server drives it through the
// script methods initdec20terminal / attachtodec20terminal / detachdec20terminal,
// which send the 'I' / 'D' / 'E' client commands handled here.
void Dec20_Init();
void Dec20_Attach(int localClientNum);
void Dec20_Exit(int localClientNum);
void Dec20_Update();
void Dec20_Reset();
void Dec20_Shutdown();
void Dec20_KeyEvent(int localClientNum, int key, int down, unsigned int time);
void Dec20_CharEvent(int localClientNum, int ch);
// Owner draw 356 in the dec20_terminal menu.
void Dec20_Paint(int localClientNum, const rectDef_s *rect, Font_s *font, float scale, int textStyle);
