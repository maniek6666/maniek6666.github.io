/*
 * Copyright (C) 2021 skgleba
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/devctl.h>
#include <psp2/ctrl.h>
#include <psp2/shellutil.h>
#include <psp2/net/http.h>
#include <psp2/net/net.h>
#include <psp2/sysmodule.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/net/netctl.h>
#include <psp2/io/stat.h>
#include <psp2/io/dirent.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "graphics.h"
#include "Archives.h"
#include "crc32.c"

#define printf psvDebugScreenPrintf

//#define DEBUG

#ifdef DEBUG
#define cprintf sceClibPrintf
#else
#define cprintf(...)
#endif

#define COLORPRINTF(color, ...)                \
	do {                                       \
		psvDebugScreenSetFgColor(color);       \
		printf(__VA_ARGS__);     \
		psvDebugScreenSetFgColor(COLOR_WHITE); \
	} while (0)

#define CHUNK_SIZE 64 * 1024
#define hasEndSlash(path) (path[strlen(path) - 1] == '/')

#include "ops.c" // misc, too clogged otherwise

#define HEN_REPO_URL "http://deploy.psp2.dev/dlc/"
#define VDEP_VPK_FNAME "vdep.vpk"
#define TAIHEN_K_FNAME "taihen.skprx"
#define TAIHEN_C_FNAME "config.txt" // default config.txt
#define HENKAKU_K_FNAME "henkaku.skprx"
#define HENKAKU_U_FNAME "henkaku.suprx"
#define TEMP_UX0_PATH "ux0:temp/"
#define TEMP_UR0_PATH "ur0:bgdl/"

#define BOOTSTRAP_VERSION_STR "henlo-bootstrap v1.0 by skgleba"

#define OPTION_COUNT 5
enum E_MENU_OPTIONS {
    MENU_EXIT = 0,
    MENU_INSTALL_HENKEK,
    MENU_INSTALL_VDEP,
    MENU_REPLACE_NEAR,
    MENU_RESET_TAICFG
};
const char* menu_items[OPTION_COUNT] = { " -> Wyjdz", " -> Zainstaluj HENkaku", " -> Zainstaluj  VitaDeploy", " -> Zastap NEAR na VitaDeploy", " -> Resetowanie taihen config.txt" };

int __attribute__((naked, noinline)) call_syscall(int a1, int a2, int a3, int num) {
    __asm__(
        "mov r12, %0 \n"
        "svc 0 \n"
        "bx lr \n"
        : : "r" (num)
    );
}

int unzip(const char* src, const char* dst) {
    Zip* handle = ZipOpen(src);
    int ret = ZipExtract(handle, NULL, dst);
    ZipClose(handle);
    return ret;
}

int install_vitadeploy_default() {
    sceIoMkdir(TEMP_UX0_PATH, 0777);
    COLORPRINTF(COLOR_CYAN, "Pobieranie vitadeploy\n");
    net(1);
    int res = download_file(HEN_REPO_URL VDEP_VPK_FNAME, TEMP_UX0_PATH VDEP_VPK_FNAME, TEMP_UX0_PATH VDEP_VPK_FNAME "_tmp", 0);
    if (res < 0)
        return res;
    net(0);

    COLORPRINTF(COLOR_CYAN, "Wypakowywanie vpk\n");
    remove(TEMP_UX0_PATH "app");
    sceIoMkdir(TEMP_UX0_PATH "app", 0777);
    res = unzip(TEMP_UX0_PATH VDEP_VPK_FNAME, TEMP_UX0_PATH "app");
    if (res < 0)
        return res;

    COLORPRINTF(COLOR_CYAN, "Promowanie app\n");
    res = promoteApp(TEMP_UX0_PATH "app");
    if (res < 0)
        return res;

    COLORPRINTF(COLOR_GREEN, "Wszystko Gotowe \n");
    sceKernelDelayThread(2 * 1000 * 1000);

    return 0;
}

int vitadeploy_x_near(int syscall_id) {
    COLORPRINTF(COLOR_WHITE, "Spowoduje to zastapienie aplikacji \"near\" na aplikacje VitaDeploy.\nWymaga ponownego uruchomienia.\n\n");
    COLORPRINTF(COLOR_YELLOW, "OSTRZEZENIE: spowoduje to zresetowanie ukLad babeli\n\n");
    COLORPRINTF(COLOR_CYAN, "KWADRAT: Zmien NEAR na VitaDeploy\nTROJKAT: Przywroc NEAR\nKOLKO: Wroc\n");
    sceKernelDelayThread(0.5 * 1000 * 1000);
    SceCtrlData pad;
    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons == SCE_CTRL_SQUARE)
            break;
        else if (pad.buttons == SCE_CTRL_CIRCLE)
            return 0;
        else if (pad.buttons == SCE_CTRL_TRIANGLE) {
            printf("Przygotowanie do przywrocenia NEAR..\n");
            sceIoMkdir(TEMP_UR0_PATH, 0777);
            removeDir(TEMP_UR0_PATH "app");
            sceIoMkdir(TEMP_UR0_PATH "app", 0777);
            int ret = copyDir("vs0:app/NPXS10000/near_backup", TEMP_UR0_PATH "app");
            if (ret < 0)
                return ret;
            printf("Usuwanie VitaDeploy..\n");
            ret = call_syscall(0, 0, 0, syscall_id + 4);
            cprintf("ioMountRW ret 0x%08X\n", ret);
            ret = removeDir("vs0:app/NPXS10000");
            if (ret < 0) {
                printf("Blad 0x%08X, ponowne uruchomienie za 5 sekund\n", ret);
                sceKernelDelayThread(5 * 1000 * 1000);
                scePowerRequestColdReset();
                sceKernelDelayThread(2 * 1000 * 1000);
                printf("Process is dead\n");
                while (1) {};
            }
            printf("Przywracanie NEAR..\n");
            ret = copyDir(TEMP_UR0_PATH "app", "vs0:app/NPXS10000");
            if (ret < 0)
                printf("Blad 0x%08X\n", ret);
            printf("Usuwanie bazy danych aplikacji\n");
            sceIoRemove("ur0:shell/db/app.db");
            printf("Czyszczenie..\n");
            removeDir(TEMP_UR0_PATH "app");
            printf("Wszystko Gotowe, ponowne uruchomienie za 5 sekund\n");
            sceKernelDelayThread(5 * 1000 * 1000);
            scePowerRequestColdReset();
            sceKernelDelayThread(2 * 1000 * 1000);
            printf("Proces jest martwy\n");
            while (1) {};
        }
    }
    sceIoMkdir(TEMP_UR0_PATH, 0777);
    printf("Pobieranie VitaDeploy\n");
    net(1);
    int res = download_file(HEN_REPO_URL VDEP_VPK_FNAME, TEMP_UR0_PATH VDEP_VPK_FNAME, TEMP_UR0_PATH VDEP_VPK_FNAME "_tmp", 0);
    if (res < 0)
        return res;
    net(0);

    printf("Wypakowywanie vpk\n");
    removeDir(TEMP_UR0_PATH "app");
    sceIoMkdir(TEMP_UR0_PATH "app", 0777);
    res = unzip(TEMP_UR0_PATH VDEP_VPK_FNAME, TEMP_UR0_PATH "app");
    if (res < 0)
        return res;

    printf("Tworzenie kopii zapasowej NEAR\n");
    res = copyDir("vs0:app/NPXS10000", TEMP_UR0_PATH "app/near_backup");
    if (res < 0)
        return res;

    printf("Przygotowanie do zamiany NEAR..\n");
    sceIoRemove(TEMP_UR0_PATH "app/sce_sys/param.sfo");
    sceIoRename(TEMP_UR0_PATH "app/sce_sys/vs.sfo", TEMP_UR0_PATH "app/sce_sys/param.sfo");
    
    res = call_syscall(0, 0, 0, syscall_id + 4);
    cprintf("ioMountRW ret 0x%08X\n", res);

    printf("Zamiana NEAR\n");
    res = removeDir("vs0:app/NPXS10000");
    cprintf("Usuwanie NEAR : 0x%08X\n", res);

    res = copyDir(TEMP_UR0_PATH "app", "vs0:app/NPXS10000");
    if (res < 0)
        return res;

    printf("Usuwanie bazy danych aplikacji\n");
    sceIoRemove("ur0:shell/db/app.db");

    printf("Czyszczenie..\n");
    removeDir(TEMP_UR0_PATH "app");

    printf("Wszystko Gotowe, ponowne uruchomienie za 5 sekund\n");
    sceKernelDelayThread(5 * 1000 * 1000);
    scePowerRequestColdReset();
    sceKernelDelayThread(2 * 1000 * 1000);
    printf("Process is dead\n");
    while (1) {};
}

int install_henkaku(void) {
    COLORPRINTF(COLOR_CYAN, "Przygotowanie ur0:tai/\n");
    sceIoMkdir("ur0:tai", 0777);
    int fd = sceIoOpen("ux0:tai/config.txt", SCE_O_RDONLY, 0);
    if (fd >= 0) {
        sceIoClose(fd);
        fcp("ux0:tai/config.txt", "ux0:tai/config.txt_old");
        sceIoRemove("ux0:tai/config.txt");
    }
    int already_taid = 0;
    fd = sceIoOpen("ur0:tai/config.txt", SCE_O_RDONLY, 0);
    if (fd >= 0) {
        sceIoClose(fd);
        already_taid = 1;
    }
    COLORPRINTF(COLOR_CYAN, "Pobieranie najnowszej wersji taihen\n");
    int ret = download_file(HEN_REPO_URL TAIHEN_K_FNAME, "ur0:tai/" TAIHEN_K_FNAME, "ur0:tai/" TAIHEN_K_FNAME "_tmp", 0);
    if (ret < 0)
        return ret;
    COLORPRINTF(COLOR_CYAN, "Pobieranie najnowszej kompilacji HENkaku\n");
    ret = download_file(HEN_REPO_URL HENKAKU_K_FNAME, "ur0:tai/" HENKAKU_K_FNAME, "ur0:tai/" HENKAKU_K_FNAME "_tmp", 0);
    if (ret < 0)
        return ret;
    ret = download_file(HEN_REPO_URL HENKAKU_U_FNAME, "ur0:tai/" HENKAKU_U_FNAME, "ur0:tai/" HENKAKU_U_FNAME "_tmp", 0);
    if (ret < 0)
        return ret;
    if (!already_taid) {
        COLORPRINTF(COLOR_CYAN, "Pobieranie domyslnej konfiguracji taihen\n");
        ret = download_file(HEN_REPO_URL TAIHEN_C_FNAME, "ur0:tai/" TAIHEN_C_FNAME, "ur0:tai/" TAIHEN_C_FNAME "_tmp", 0);
        if (ret < 0)
            return ret;
    }
    COLORPRINTF(COLOR_GREEN, "Wszystko Gotowe\n");
    sceKernelDelayThread(2 * 1000 * 1000);
    return 0;
}

void main_menu(int sel) {
    psvDebugScreenClear(COLOR_BLACK);
    COLORPRINTF(COLOR_YELLOW, BOOTSTRAP_VERSION_STR "\n");
    COLORPRINTF(COLOR_WHITE, "\n---------------------------------------------\n\n");
    for (int i = 0; i < OPTION_COUNT; i++) {
        if (sel == i)
            psvDebugScreenSetFgColor(COLOR_CYAN);
        printf("%s\n", menu_items[i]);
        psvDebugScreenSetFgColor(COLOR_WHITE);
    }
    psvDebugScreenSetFgColor(COLOR_WHITE);
    COLORPRINTF(COLOR_WHITE, "\n---------------------------------------------\n\n");
}

int _start(SceSize args, void* argp) {
    int syscall_id = *(uint16_t*)argp;

    cprintf("Ladowanie PAF\n");
    int res = load_sce_paf();
    if (res < 0)
        goto EXIT;

    psvDebugScreenInit();
    COLORPRINTF(COLOR_CYAN, BOOTSTRAP_VERSION_STR "\n");

    int sel = 0;
    SceCtrlData pad;
    main_menu(sel);
    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons == SCE_CTRL_CROSS) {
            res = -1;
            if (sel == MENU_EXIT) {
                res = 0;
                goto EXIT;
            } else if (sel == MENU_INSTALL_VDEP) {
                res = install_vitadeploy_default();
                if (res < 0) {
                    COLORPRINTF(COLOR_RED, "BLAD: 0x%08X\n", res);
                    sceKernelDelayThread(3 * 1000 * 1000);
                }
                sel = 0;
                main_menu(sel);
                sceKernelDelayThread(0.3 * 1000 * 1000);
            } else if (sel == MENU_REPLACE_NEAR) {
                res = vitadeploy_x_near(syscall_id);
                if (res < 0) {
                    COLORPRINTF(COLOR_RED, "BLAD: 0x%08X\n", res);
                    sceKernelDelayThread(3 * 1000 * 1000);
                }
                sel = 0;
                main_menu(sel);
                sceKernelDelayThread(0.3 * 1000 * 1000);
            } else if (sel == MENU_INSTALL_HENKEK) {
                net(1);
                res = install_henkaku();
                net(0);
                if (res < 0) {
                    COLORPRINTF(COLOR_RED, "BLAD: 0x%08X\n", res);
                    sceKernelDelayThread(3 * 1000 * 1000);
                }
                sel = 0;
                main_menu(sel);
                sceKernelDelayThread(0.3 * 1000 * 1000);
            } else if (sel == MENU_RESET_TAICFG) {
                COLORPRINTF(COLOR_CYAN, "Pobieranie domyslnej konfiguracji taihen\n");
                sceIoMkdir("ur0:tai", 0777);
                net(1);
                res = download_file(HEN_REPO_URL TAIHEN_C_FNAME, "ur0:tai/" TAIHEN_C_FNAME, "ur0:tai/" TAIHEN_C_FNAME "_tmp", 0);
                net(0);
                if (res < 0) {
                    COLORPRINTF(COLOR_RED, "BLAD: 0x%08X\n", res);
                    sceKernelDelayThread(3 * 1000 * 1000);
                } else {
                    COLORPRINTF(COLOR_GREEN, "Wszystko Gotowe\n");
                    sceKernelDelayThread(2 * 1000 * 1000);
                }
                sel = 0;
                main_menu(sel);
                sceKernelDelayThread(0.3 * 1000 * 1000);
            }
        } else if (pad.buttons == SCE_CTRL_UP) {
            if (sel != 0)
                sel--;
            main_menu(sel);
            sceKernelDelayThread(0.3 * 1000 * 1000);
        } else if (pad.buttons == SCE_CTRL_DOWN) {
            if (sel + 1 < OPTION_COUNT)
                sel++;
            main_menu(sel);
            sceKernelDelayThread(0.3 * 1000 * 1000);
        }
    }

    printf("Wszystko Gotowe!\n");

EXIT:
    cprintf("Wyjscie z res 0x%08X\n", res);
    printf("Wyjscie za 3 Sekundy\n");
    sceKernelDelayThread(3 * 1000 * 1000);
    
    // Remove pkg patches
    cprintf("Usuwanie pliku pkg.. \n");
    res = call_syscall(0, 0, 0, syscall_id + 1);
    if (res >= 0) {
        // Start HENkaku
        cprintf("Henkeks.. \n");
        printf("\nRozpoczecie tworzenia struktury taihen...\n\n\nJesli utknales na tym ekranie:\n\n - Wymus ponowne uruchomienie, przytrzymujac przycisk zasilania\n\n - Ponownie uruchom Exploita\n\n - Przytrzymaj lewy Trigger [LT] podczas wychodzenia\n\n\n\nJesli problem bedzie sie powtarzal, zresetuj plik taihen config.txt za pomocą zaladowanego menu bootstrap\n");
        res = call_syscall(0, 0, 0, syscall_id + 0);
        psvDebugScreenClear(COLOR_BLACK);
    } else {
        // Remove sig patches
        cprintf("Usuwanie patczki sig\n");
        call_syscall(0, 0, 0, syscall_id + 2);
    }

    if (res < 0 && res != 0x8002D013 && res != 0x8002D017) {
        COLORPRINTF(COLOR_YELLOW, BOOTSTRAP_VERSION_STR "\n");
        COLORPRINTF(COLOR_WHITE, "\n---------------------------------------------\n\n");
        printf(" > Nie udalo sie uruchomic taihen! 0x%08X\n", res);
        printf(" > Uruchom ponownie Exploita i wybierz „Zainstaluj HENkaku”.\n");
    }

    // Clean up
    cprintf("Czyszczenie.. \n");
    call_syscall(0, 0, 0, syscall_id + 3);

    cprintf("Wszystko Gotowe, Wychodzenie\n");

    cprintf("Wypakowywanie PAF\n");
    unload_sce_paf();

    sceKernelExitProcess(0);
    return 0;
}