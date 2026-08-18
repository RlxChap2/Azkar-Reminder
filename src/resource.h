#pragma once

// ── Resource IDs ────────────────────────────────────────────────────────────
#define IDI_APP_ICON          101
#define IDI_APP_ICON_SMALL    102
#define IDR_REMINDER_SOUND    103
#define IDR_LOGO              200   // PNG logo, embedded in .rc, loaded via WIC
#define IDR_CONTENT_MD        201   // Markdown content embedded in .rc
#define IDR_MANIFEST          1

// ── Tray menu IDs ───────────────────────────────────────────────────────────
#define ID_TRAY_OPEN          2001
#define ID_TRAY_PAUSE         2002
#define ID_TRAY_RESUME        2003
#define ID_TRAY_NEXT          2004
#define ID_TRAY_SETTINGS      2005
#define ID_TRAY_STATISTICS    2006
#define ID_TRAY_ABOUT         2007
#define ID_TRAY_EXIT          2008

// Pause submenu
#define ID_TRAY_PAUSE_15      2010
#define ID_TRAY_PAUSE_30      2011
#define ID_TRAY_PAUSE_60      2012
#define ID_TRAY_PAUSE_120     2013
#define ID_TRAY_PAUSE_TODAY   2014

// ── Custom window messages ───────────────────────────────────────────────────
#define WM_TRAY_CALLBACK      (WM_APP + 1)
#define WM_REMINDER_TRIGGER   (WM_APP + 2)
#define WM_SETTINGS_CHANGED   (WM_APP + 3)
#define WM_THEME_CHANGED      (WM_APP + 4)
#define WM_PAUSE_CHANGED      (WM_APP + 5)
#define WM_CONTENT_UPDATED    (WM_APP + 6)
#define WM_NAVIGATE_PAGE      (WM_APP + 7)
#define WM_POPUP_CLOSE        (WM_APP + 8)
#define WM_UPDATER_RESULT     (WM_APP + 9)
#define WM_CHECK_FOR_UPDATES  (WM_APP + 10)
#define WM_UPDATE_AVAILABLE   (WM_APP + 12)

// ── Dialog Control IDs ──────────────────────────────────────────────────────
#define IDC_STATIC_TEXT         1001
#define IDC_BTN_OK              1002
#define IDC_BTN_CANCEL          1003
#define IDC_CHECK_ONBOARDING    1004

// ── Onboarding dialog ────────────────────────────────────────────────────────
#define IDD_ONBOARDING          300
#define IDC_ONBOARD_TITLE       301
#define IDC_ONBOARD_BODY        302
#define IDC_ONBOARD_NEXT        303
#define IDC_ONBOARD_BACK        304
#define IDC_ONBOARD_BROWSE      305
#define IDC_ONBOARD_FILE_LABEL  306

// ── Timer IDs ───────────────────────────────────────────────────────────────
#define TIMER_POPUP_ANIM_ENTER   1
#define TIMER_POPUP_ANIM_EXIT    2
#define TIMER_POPUP_AUTODISMISS  3
#define TIMER_THEME_TRANSITION   4
#define TIMER_NAV_HOVER          5

// ── Page IDs ────────────────────────────────────────────────────────────────
#define PAGE_HOME         0
#define PAGE_REMINDERS    1
#define PAGE_SCHEDULE     2
#define PAGE_LIBRARY      3
#define PAGE_STATISTICS   4
#define PAGE_SETTINGS     5
#define PAGE_ABOUT        6
