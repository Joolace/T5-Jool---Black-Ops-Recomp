#pragma once

int __cdecl Win_InitLocalization();
// localization.txt's language, or the first installed one when it is not.
void Win_PickInstalledLanguage();
void __cdecl Win_ShutdownLocalization();
char *__cdecl Win_LocalizeRef(const char *ref);
char *__cdecl Win_CopyLocalizationString(const char *string);
char *__cdecl Win_GetLanguage();
