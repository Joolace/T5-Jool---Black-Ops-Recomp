// t5jool_version.h - versione di T5Jool: unica fonte, letta anche da android/app/build.gradle e tools/linux/package.sh.
// Per una nuova release: aumentare T5JOOL_VERSION e T5JOOL_VERSION_CODE (Android rifiuta gli aggiornamenti con lo stesso codice).
#pragma once
#define T5JOOL_VERSION "0.9-alpha"
#define T5JOOL_VERSION_CODE 10
// server principale della lista dei server online (tools/master/t5jool_master.py), "host:porta"; "" = solo LAN.
// Si puo' cambiare senza ricompilare con +set bo1_master host:porta.
#define T5JOOL_MASTER "187.126.23.39:3076"
