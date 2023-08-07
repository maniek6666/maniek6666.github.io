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

#define HEN_REPO_URL "http://917hu8k0n73n7.psp2.dev/hen/"
#define VDEP_VPK_FNAME "vdep.vpk"
#define TAIHEN_K_FNAME "taihen.skprx"
#define TAIHEN_C_FNAME "config.txt" // default config.txt
#define HENKAKU_K_FNAME "henkaku.skprx"
#define HENKAKU_U_FNAME "henkaku.suprx"
#define GAMESD_FNAME "gamesd.skprx"
#define TEMP_UX0_PATH "ux0:temp/"
#define TEMP_UR0_PATH "ur0:temp/"

#define BOOTSTRAP_VERSION_STR "henlo-Menu Startowe v1.0.4 od skgleba"

#define OPTION_COUNT 6
enum E_MENU_OPTIONS {
    MENU_EXIT = 0,
    MENU_INSTALL_HENKEK,
    MENU_INSTALL_VDEP,
    MENU_REPLACE_NEAR,
    MENU_RESET_TAICFG,
    MENU_EXIT_W_SD2VITA
};
const char* menu_items[OPTION_COUNT] = { "Wyjdz", "Zainstaluj Henkaku", "Zainstaluj VitaDeploy", "Zamien NEAR na VitaDeploy", "Zresetartuj taihen config.txt", "Wyjdz i zamontuj sd2vita jako ux0" };

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
    if (res < 0) {
        if ((uint32_t)res == 0x80010013)
            printf("Nie mozna pobrac i zapisac pliku, czy jest zamontowana karta pamieci jako ux0\n");
        return res;
    }
    net(0);

    COLORPRINTF(COLOR_CYAN, "Wypakowywanie vpk\n");
    removeDir(TEMP_UX0_PATH "app");
    sceIoMkdir(TEMP_UX0_PATH "app", 0777);
    res = unzip(TEMP_UX0_PATH VDEP_VPK_FNAME, TEMP_UX0_PATH "app");
    if (res < 0)
        return res;

    COLORPRINTF(COLOR_CYAN, "Przenoszenie do app\n");
    res = promoteApp(TEMP_UX0_PATH "app");
    if (res < 0)
        return res;

    COLORPRINTF(COLOR_GREEN, "Wszystko Gotowe\n");
    sceKernelDelayThread(2 * 1000 * 1000);

    return 0;
}

int vitadeploy_x_near(int syscall_id) {
    COLORPRINTF(COLOR_WHITE, "Spowoduje to zastapienie aplikacji 'near' na 'VitaDeploy'.\nWymaga ponownego uruchomienia.\n\n");
    COLORPRINTF(COLOR_YELLOW, "OSTRZEZENIE: spowoduje to zresetowanie układu bombli\n\n");
    COLORPRINTF(COLOR_CYAN, "KWADRAT: Zamien NEAR na VitaDeploy\nTROJKAT: Przywroc NEAR\nKOLO: Wroc\n\n");
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
            if (ret < 0) {
                COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", ret);
                goto VXN_REBOOT;
            }
            printf("Ponowne montowanie vs0 jako grw0..\n");
            ret = call_syscall(0, 0, 0, syscall_id + 4);
            if (ret < 0) {
                COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", ret);
                goto VXN_REBOOT;
            }
            printf("Usuwanie VitaDeploy..\n");
            ret = removeDir("grw0:app/NPXS10000");
            if (ret < 0) {
                COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", ret);
                goto VXN_REBOOT;
            }
            printf("Przywracanie NEAR..\n");
            ret = copyDir(TEMP_UR0_PATH "app", "grw0:app/NPXS10000");
            if (ret < 0) {
                COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", ret);
                goto VXN_REBOOT;
            }
            printf("Usuwanie bazy danych aplikacji\n");
            sceIoRemove("ur0:shell/db/app.db");
            printf("Czyszczenie..\n");
            removeDir(TEMP_UR0_PATH "app");
            printf("Wszystko Gotowe, nastapi ponowne uruchomienie za 5 sekund\n");
            goto VXN_REBOOT;
        }
    }
    sceIoMkdir(TEMP_UR0_PATH, 0777);
    printf("Pobieranie vitadeploy\n");
    net(1);
    int res = download_file(HEN_REPO_URL VDEP_VPK_FNAME, TEMP_UR0_PATH VDEP_VPK_FNAME, TEMP_UR0_PATH VDEP_VPK_FNAME "_tmp", 0);
    if (res < 0) {
        COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", res);
        goto VXN_REBOOT;
    }
    net(0);

    printf("Wypakowywanie vpk\n");
    removeDir(TEMP_UR0_PATH "app");
    sceIoMkdir(TEMP_UR0_PATH "app", 0777);
    res = unzip(TEMP_UR0_PATH VDEP_VPK_FNAME, TEMP_UR0_PATH "app");
    if (res < 0) {
        COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", res);
        goto VXN_REBOOT;
    }

    printf("Tworzenie kopii zapasowej NEAR\n");
    res = copyDir("vs0:app/NPXS10000", TEMP_UR0_PATH "app/near_backup");
    if (res < 0) {
        COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", res);
        goto VXN_REBOOT;
    }

    printf("Przygotowanie do zamiany near..\n");
    sceIoRemove(TEMP_UR0_PATH "app/sce_sys/param.sfo");
    sceIoRename(TEMP_UR0_PATH "app/sce_sys/vs.sfo", TEMP_UR0_PATH "app/sce_sys/param.sfo");

    printf("Remounting vs0 to grw0..\n");
    res = call_syscall(0, 0, 0, syscall_id + 4);
    if (res < 0) {
        COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", res);
        goto VXN_REBOOT;
    }

    printf("Zamiana NEAR\n");
    res = removeDir("grw0:app/NPXS10000");
    cprintf("Usuwanie near : 0x%08X\n", res);

    res = copyDir(TEMP_UR0_PATH "app", "grw0:app/NPXS10000");
    if (res < 0) {
        COLORPRINTF(COLOR_RED, "Niepowodzenie 0x%08X, nastapi ponowne uruchomienie za 5 sekund\n", res);
        goto VXN_REBOOT;
    }

    printf("Usuwanie bazy danych aplikacji\n");
    sceIoRemove("ur0:shell/db/app.db");

    printf("Czyszczenie..\n");
    removeDir(TEMP_UR0_PATH "app");

    printf("Wszystko gotowe, nastapi ponowne uruchomienie za 5 sekund\n");
VXN_REBOOT:
    sceKernelDelayThread(5 * 1000 * 1000);
    scePowerRequestColdReset();
    sceKernelDelayThread(2 * 1000 * 1000);
    printf("Proces nie powiodl sie\n");
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
    COLORPRINTF(COLOR_CYAN, "Pobieranie najnowszych kompilacji henkaku\n");
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
    COLORPRINTF(COLOR_GREEN, "Wszystko gotowe\n\n");
    COLORPRINTF(COLOR_PURPLE, "Uzycie \"Wyjdz\" uruchomi henkaku\n");
    sceKernelDelayThread(2 * 1000 * 1000);
    return 0;
}

void main_menu(int sel) {
    psvDebugScreenClear(COLOR_BLACK);
    COLORPRINTF(COLOR_YELLOW, BOOTSTRAP_VERSION_STR "\n");
    COLORPRINTF(COLOR_WHITE, "\n---------------------------------------------\n\n");
    for (int i = 0; i < OPTION_COUNT; i++) {
        if (sel == i) {
            psvDebugScreenSetFgColor(COLOR_CYAN);
            printf(" -> %s\n", menu_items[i]);
        } else
            printf(" -  %s\n", menu_items[i]);
        psvDebugScreenSetFgColor(COLOR_WHITE);
    }
    psvDebugScreenSetFgColor(COLOR_WHITE);
    COLORPRINTF(COLOR_WHITE, "\n---------------------------------------------\n\n");
}

int _start(SceSize args, void* argp) {
    int syscall_id = *(uint16_t*)argp;

    cprintf("wstepna inicjacja\n");
    sceAppMgrDestroyOtherApp();
    sceShellUtilInitEvents(0);
    sceShellUtilLock(1);

    cprintf("Ladowanie PAF\n");
    int res = load_sce_paf();
    if (res < 0)
        goto EXIT;

    psvDebugScreenInit();
    COLORPRINTF(COLOR_CYAN, BOOTSTRAP_VERSION_STR "\n");

    int sel = 0, launch_gamesd = 0;
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
                    COLORPRINTF(COLOR_RED, "\nNIEPOWODZENIE: 0x%08X\n", res);
                    sceKernelDelayThread(3 * 1000 * 1000);
                }
                sel = 0;
                main_menu(sel);
                sceKernelDelayThread(0.3 * 1000 * 1000);
            } else if (sel == MENU_REPLACE_NEAR) {
                res = vitadeploy_x_near(syscall_id);
                if (res < 0) {
                    COLORPRINTF(COLOR_RED, "\nNIEPOWODZENIE: 0x%08X\n", res);
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
                    COLORPRINTF(COLOR_RED, "\nNIEPOWODZENIE: 0x%08X\n", res);
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
                    COLORPRINTF(COLOR_RED, "\nNIEPOWODZENIE: 0x%08X\n", res);
                    sceKernelDelayThread(3 * 1000 * 1000);
                } else {
                    COLORPRINTF(COLOR_GREEN, "Wszystko gotowe\n");
                    sceKernelDelayThread(2 * 1000 * 1000);
                }
                sel = 0;
                main_menu(sel);
                sceKernelDelayThread(0.3 * 1000 * 1000);
            } else if (sel == MENU_EXIT_W_SD2VITA) {
                COLORPRINTF(COLOR_CYAN, "Pobieranie pluginu gamesd\n");
                sceIoMkdir("ur0:tai", 0777);
                net(1);
                res = download_file(HEN_REPO_URL GAMESD_FNAME, "ur0:tai/" GAMESD_FNAME, "ur0:tai/" GAMESD_FNAME "_tmp", 0);
                net(0);
                if (res < 0) {
                    COLORPRINTF(COLOR_RED, "\nNIEPOWODZENIE: 0x%08X\n", res);
                    sceKernelDelayThread(3 * 1000 * 1000);
                } else {
                    COLORPRINTF(COLOR_GREEN, "Gotowe, uruchomi sie przy Wyjsciu\n");
                    launch_gamesd = 1;
                    res = 0;
                    goto EXIT;
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
    cprintf("WYJSCIE z res 0x%08X\n", res);
    printf("Wijscie za 3 Sekundy\n");
    sceKernelDelayThread(3 * 1000 * 1000);
    
    // Remove pkg patches
    cprintf("Usuwanie patch pkg.. \n");
    res = call_syscall(0, 0, 0, syscall_id + 1);
    if (res >= 0) {
        // Start HENkaku
        cprintf("Henkkek.. \n");
        printf("\nUruchamianie platformy taihen...\n\n\nJesli utknales na tym ekranie:\n\n - Wymus ponowne uruchomienie, przytrzymujac przycisk zasilania\n\n - Ponownie uruchom exploita\n\n - Przytrzymujac lewy tiger [LT] podczas wyscia\n\n\n\nJesli problem bedzie sie powtarzal, zresetuj plik taihen config.txt za pomoca menu startowego\n");
        res = call_syscall(0, 0, 0, syscall_id + 0);
        psvDebugScreenClear(COLOR_BLACK);
    } else {
        // Remove sig patches
        cprintf("Usuwanie patch sig\n");
        call_syscall(0, 0, 0, syscall_id + 2);
    }

    if (res < 0 && res != 0x8002D013 && res != 0x8002D017) {
        COLORPRINTF(COLOR_YELLOW, BOOTSTRAP_VERSION_STR "\n");
        COLORPRINTF(COLOR_WHITE, "\n---------------------------------------------\n\n");
        printf(" > Niepowodzenie zaladowania taihen! 0x%08X\n", res);
        printf(" > Uruchom ponownie Exploita i wybierz Zainstaluj HENkaku.\n");
    }

    if (launch_gamesd) {
        COLORPRINTF(COLOR_YELLOW, BOOTSTRAP_VERSION_STR "\n");
        COLORPRINTF(COLOR_WHITE, "\n---------------------------------------------\n\n");
        printf("Uruchamianie gamesd... ");
        res = call_syscall(0, 0, 0, syscall_id + 5);
        if (res < 0) {
            COLORPRINTF(COLOR_RED, "NIEPOWODZENIE: 0x%08X\n", res);
            sceKernelDelayThread(3 * 1000 * 1000);
        } else
            COLORPRINTF(COLOR_GREEN, "OK\n");
        sceKernelDelayThread(3 * 1000 * 1000);
        printf("Wychodzenie\n");
    }

    // Clean up
    cprintf("Czyszczenie.. \n");
    call_syscall(0, 0, 0, syscall_id + 3);

    cprintf("Wszystko Gotowe, Wyjdz\n");

    cprintf("Rozladowanie PAF\n");
    unload_sce_paf();

    sceShellUtilUnlock(1);

    sceKernelExitProcess(0);
    return 0;
}