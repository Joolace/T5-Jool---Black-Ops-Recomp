// bo1_feel.h - T5Jool: vibrazione (controller o, su Android, il motore del dispositivo) e mira giroscopica.
// Dvar (Options > Controls): bo1_rumble, bo1_rumble_strength, bo1_gyro (0 off, 1 sempre, 2 mirando),
// bo1_gyro_sens, bo1_gyro_invert_x, bo1_gyro_invert_y.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif
// impulso: low = motore grande (colpi forti), high = motore piccolo (vibrazione fine), 0..1, durata in ms
void BO1Rumble_Pulse(float low, float high, int ms);
// vibrazione continua per questo frame (scosse della telecamera: esplosioni); va richiamata ogni frame
void BO1Rumble_Continuous(float level);
// aggiorna i motori (una volta per frame)
void BO1Rumble_Frame(void);
// rotazione da aggiungere alla vista in questo frame (gradi); aiming = mira con il mirino
void BO1Gyro_Apply(float *yawDelta, float *pitchDelta, int aiming);
#ifdef __cplusplus
}
#endif
