// bo1_android.h - funzioni solo Android (vedi bo1_android.cpp)
#pragma once
#if defined(__ANDROID__)
void bo1_android_init(int *argc, char **argv); // toglie gli argomenti --t5-*, log, gestore crash, driver Vulkan
const char *bo1_android_home();                // cartella T5Jool sulla memoria condivisa
void bo1_android_clean_exit();                 // uscita pulita: cancella il file "partita in corso"
// processo nuovo per la prossima mappa (memoria bassa intera): il gioco si chiude e riparte eseguendo cmd
// ("set systemlink 1;devmap zombie_theater"). false se non si e' potuto chiedere il riavvio.
bool bo1_android_relaunch(const char *cmd);
const char *bo1_android_graphics_hint();       // testo per "grafica non partita" (Mali senza PanVK), o nullptr
void bo1_android_screen_drawn();                // il gioco disegna lo schermo (menu o caricamento)
void bo1_android_frame_presented();             // ogni Present: due frame dopo il primo disegno toglie la schermata di caricamento (GameActivity.gameReady)
void bo1_android_dump_threads();                // stack di tutti i thread del gioco in game.txt (avvio bloccato)
#endif
