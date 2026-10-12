#pragma once

// ── Resource IDs ────────────────────────────────────────────────────────────
#define IDI_APP_ICON          101
#define IDI_APP_ICON_SMALL    102
#define IDI_APP_DORMANT_ICON  104
#define IDI_APP_DORMANT_SMALL 105
#define IDI_APP_YELLOW_ICON   106
#define IDI_APP_YELLOW_SMALL  107
#define IDI_APP_BLUE_ICON     108
#define IDI_APP_BLUE_SMALL    109
#define IDI_APP_RED_ICON      110
#define IDI_APP_RED_SMALL     111
#define IDI_APP_SILVER_ICON   112
#define IDI_APP_SILVER_SMALL  113
#define IDI_APP_GOLD_ICON     114
#define IDI_APP_GOLD_SMALL    115
#define IDR_REMINDER_SOUND    103
#define IDR_LOGO              200   // PNG logo, embedded in .rc, loaded via WIC
#define IDR_CONTENT_MD        201   // Markdown content embedded in .rc
#define IDR_LOGO_SMALL        202   // High-performance cropped circular logo
#define IDR_FONT_CAIRO        203   // Embedded Cairo Arabic font
#define IDR_QR_VODAFONE       204   // Vodafone Cash QR image
#define IDR_QR_IBAN           205   // Banque Misr IBAN QR image
#define IDR_LOGO_DORMANT      206   // Silver/chrome dormant logo
#define IDR_LOGO_DORMANT_SMALL 207  // Cropped silver/chrome dormant logo
#define IDR_LOGO_YELLOW       208
#define IDR_LOGO_YELLOW_SMALL 209
#define IDR_LOGO_BLUE         210
#define IDR_LOGO_BLUE_SMALL   211
#define IDR_LOGO_RED          212
#define IDR_LOGO_RED_SMALL    213
#define IDR_LOGO_SILVER       214
#define IDR_LOGO_SILVER_SMALL 215
#define IDR_LOGO_GOLD         216
#define IDR_LOGO_GOLD_SMALL   217
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
#define PAGE_LIBRARY      2
#define PAGE_SETTINGS     3
#define PAGE_ABOUT        4
#define PAGE_COUNT        5
