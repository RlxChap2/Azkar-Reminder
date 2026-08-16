#include "Localization.h"
#include <unordered_map>

namespace dhikr {

// Static string maps for fast in-memory access
static std::unordered_map<std::wstring, std::wstring> g_en = {
    // Navigation
    { L"nav.home", L"Home" },
    { L"nav.reminders", L"Reminders" },
    { L"nav.schedule", L"Schedule" },
    { L"nav.library", L"Dhikr Library" },
    { L"nav.statistics", L"Statistics" },
    { L"nav.settings", L"Settings" },
    { L"nav.about", L"About" },

    // Home
    { L"home.greeting", L"As-salamu alaykum" },
    { L"home.subtitle", L"May Allah accept your remembrance." },
    { L"home.status", L"Status:" },
    { L"home.active", L"Active" },
    { L"home.paused", L"Paused" },
    { L"home.next_reminder", L"Next Reminder" },
    { L"home.todays_reminders", L"Today's Reminders" },
    { L"home.current_streak", L"Current Streak" },
    { L"home.total_reminders", L"Total Reminders" },
    { L"home.todays_progress", L"Today's Progress" },
    { L"home.days", L"Days" },
    { L"home.this_week", L"This Week" },
    { L"home.completed", L"Completed" },
    { L"home.in_minutes", L"In minutes" },
    { L"home.today", L"Today" },
    { L"home.total", L"Total" },
    { L"home.streak", L"Streak" },
    { L"home.minutes", L"min" },

    // Reminders
    { L"reminders.title", L"Reminders" },
    { L"reminders.enable", L"Enable Reminders" },
    { L"reminders.start_time", L"Start Time" },
    { L"reminders.end_time", L"End Time" },
    { L"reminders.frequency", L"Frequency" },
    { L"reminders.quiet_hours", L"Quiet Hours" },
    { L"reminders.pause_for", L"Pause For" },
    { L"reminders.preview", L"Preview Notification" },
    { L"reminders.save", L"Save Changes" },
    { L"reminders.save_star", L"Save Changes *" },
    { L"reminders.pause_dropdown", L"Pause" },

    // Schedule
    { L"schedule.title", L"Weekly Schedule" },
    { L"schedule.sunday", L"Sunday" },
    { L"schedule.monday", L"Monday" },
    { L"schedule.tuesday", L"Tuesday" },
    { L"schedule.wednesday", L"Wednesday" },
    { L"schedule.thursday", L"Thursday" },
    { L"schedule.friday", L"Friday" },
    { L"schedule.saturday", L"Saturday" },
    { L"schedule.sun", L"Sun" },
    { L"schedule.mon", L"Mon" },
    { L"schedule.tue", L"Tue" },
    { L"schedule.wed", L"Wed" },
    { L"schedule.thu", L"Thu" },
    { L"schedule.fri", L"Fri" },
    { L"schedule.sat", L"Sat" },
    { L"schedule.add_quiet_hours", L"Add Quiet Hours" },

    // Library
    { L"library.title", L"Dhikr Library" },
    { L"library.add_dhikr", L"+ Add Dhikr" },
    { L"library.dialog_title", L"Add Custom Dhikr" },
    { L"library.dhikr_text", L"Dhikr Text" },
    { L"library.category", L"Category" },
    { L"library.repeat_count", L"Repetitions" },
    { L"library.filter_all", L"All" },
    { L"library.filter_morning_evening", L"Morning & Evening" },
    { L"library.filter_tasbih", L"Tasbih" },
    { L"library.filter_custom", L"Custom" },
    { L"library.delete_all", L"Delete All" },
    { L"library.import_md", L"Import MD" },
    { L"library.empty", L"Your library is empty. Add a dhikr or import a markdown file." },
    { L"library.delete_confirm", L"Are you sure you want to delete all dhikr items?" },
    { L"library.delete_title", L"Confirm Delete" },
    { L"library.delete_item_confirm", L"Delete this dhikr item?" },
    { L"library.import_success", L"Dhikr items imported successfully!" },
    { L"library.import_error", L"Failed to import markdown file." },
    { L"library.items_loaded", L"Items available" },

    // Statistics
    { L"statistics.title", L"Statistics" },
    { L"statistics.today", L"Today" },
    { L"statistics.this_week", L"This Week" },
    { L"statistics.this_month", L"This Month" },
    { L"statistics.total", L"Total Reminders" },
    { L"statistics.completed", L"Completed" },
    { L"statistics.streak", L"Streak" },
    { L"statistics.progress", L"Progress" },
    { L"stats.daily_chart", L"Daily Activity" },
    { L"stats.top_dhikrs", L"Top Dhikr Items" },

    // Popup
    { L"popup.copy", L"Copy" },
    { L"popup.copied", L"Copied!" },
    { L"popup.snooze", L"Snooze 5m" },
    { L"popup.dismiss", L"Dismiss" },
    { L"home.next_preview_prefix", L"Upcoming Dhikr:" },

    // Settings
    { L"settings.title", L"Settings" },
    { L"settings.general", L"General" },
    { L"settings.popup", L"Popup" },
    { L"settings.sound", L"Sound" },
    { L"settings.system", L"System" },
    { L"settings.language", L"Language" },
    { L"settings.start_with_windows", L"Start with Windows" },
    { L"settings.accent_color", L"Accent Color" },
    { L"settings.reset_all", L"Reset All Settings" },
    { L"settings.position", L"Position" },
    { L"settings.monitor", L"Monitor" },
    { L"settings.duration", L"Duration" },
    { L"settings.animations", L"Animations" },
    { L"settings.enable_sound", L"Enable Sound" },
    { L"settings.volume", L"Volume" },
    { L"settings.notification_sound", L"Notification Sound" },
    { L"settings.sound_default", L"Default (Chime)" },
    { L"settings.choose_sound", L"Browse Audio File..." },
    { L"settings.reset_sound", L"Reset to Default" },
    { L"settings.test_sound", L"Test Sound" },
    { L"settings.version", L"Application Version" },
    { L"settings.check_updates", L"Check for Updates" },
    { L"settings.checking", L"Checking..." },
    { L"settings.up_to_date", L"Up to date" },
    { L"settings.update_available", L"Update Available" },
    { L"settings.update_now", L"Update Now" },
    { L"settings.reset_confirm_title", L"Reset All Settings" },
    { L"settings.reset_confirm_msg", L"Are you sure you want to reset all settings to defaults? This action cannot be undone." },
    { L"settings.fullscreen_suppression", L"Do not interrupt in full-screen mode" },
    { L"settings.fullscreen_behavior", L"When full-screen active" },
    { L"settings.fullscreen_queue", L"Queue until full-screen ends" },
    { L"settings.fullscreen_suppress", L"Suppress reminder completely" },
    { L"settings.fullscreen_win_notify", L"Windows notification only" },

    // TimePicker
    { L"timepicker.title", L"Select Time" },
    { L"timepicker.apply", L"Apply" },
    { L"timepicker.cancel", L"Cancel" },

    // About
    { L"about.title", L"About" },
    { L"about.version", L"Version" },
    { L"about.contact", L"Contact Developer" },
    { L"about.copy", L"Copy" },
    { L"about.copied", L"Copied!" },

    // Tray
    { L"tray.show", L"Show Azkar Reminder" },
    { L"tray.pause", L"Pause Reminders (1h)" },
    { L"tray.resume", L"Resume Reminders" },
    { L"tray.settings", L"Settings" },
    { L"tray.exit", L"Exit" },

    // Common
    { L"common.ok", L"OK" },
    { L"common.cancel", L"Cancel" },
    { L"common.close", L"Close" },
    { L"common.yes", L"Yes" },
    { L"common.no", L"No" },
    { L"common.save", L"Save" },
    { L"common.delete", L"Delete" },
    { L"common.warning", L"Warning" },
    { L"common.error", L"Error" },
    { L"common.success", L"Success" },
};

static std::unordered_map<std::wstring, std::wstring> g_ar = {
    // Navigation
    { L"nav.home", L"الرئيسية" },
    { L"nav.reminders", L"التنبيهات" },
    { L"nav.schedule", L"الجدول" },
    { L"nav.library", L"مكتبة الأذكار" },
    { L"nav.statistics", L"الإحصائيات" },
    { L"nav.settings", L"الإعدادات" },
    { L"nav.about", L"عن التطبيق" },

    // Home
    { L"home.greeting", L"السلام عليكم" },
    { L"home.subtitle", L"تقبل الله ذكركم." },
    { L"home.status", L"الحالة:" },
    { L"home.active", L"نشط" },
    { L"home.paused", L"متوقف مؤقتاً" },
    { L"home.next_reminder", L"التنبيه القادم" },
    { L"home.todays_reminders", L"تنبيهات اليوم" },
    { L"home.current_streak", L"السلسلة الحالية" },
    { L"home.total_reminders", L"إجمالي التنبيهات" },
    { L"home.todays_progress", L"تقدم اليوم" },
    { L"home.days", L"أيام" },
    { L"home.this_week", L"هذا الأسبوع" },
    { L"home.completed", L"مكتمل" },
    { L"home.in_minutes", L"بالدقائق" },
    { L"home.today", L"اليوم" },
    { L"home.total", L"الإجمالي" },
    { L"home.streak", L"السلسلة" },
    { L"home.minutes", L"دقيقة" },

    // Reminders
    { L"reminders.title", L"التنبيهات" },
    { L"reminders.enable", L"تفعيل التنبيهات" },
    { L"reminders.start_time", L"وقت البدء" },
    { L"reminders.end_time", L"وقت الانتهاء" },
    { L"reminders.frequency", L"التكرار" },
    { L"reminders.quiet_hours", L"أوقات الهدوء" },
    { L"reminders.pause_for", L"إيقاف مؤقت لمدة" },
    { L"reminders.preview", L"تجربة التنبيه" },
    { L"reminders.save", L"حفظ التغييرات" },
    { L"reminders.save_star", L"حفظ التغييرات *" },
    { L"reminders.pause_dropdown", L"إيقاف مؤقت" },

    // Schedule
    { L"schedule.title", L"الجدول الأسبوعي" },
    { L"schedule.sunday", L"الأحد" },
    { L"schedule.monday", L"الإثنين" },
    { L"schedule.tuesday", L"الثلاثاء" },
    { L"schedule.wednesday", L"الأربعاء" },
    { L"schedule.thursday", L"الخميس" },
    { L"schedule.friday", L"الجمعة" },
    { L"schedule.saturday", L"السبت" },
    { L"schedule.sun", L"الأحد" },
    { L"schedule.mon", L"الإثنين" },
    { L"schedule.tue", L"الثلاثاء" },
    { L"schedule.wed", L"الأربعاء" },
    { L"schedule.thu", L"الخميس" },
    { L"schedule.fri", L"الجمعة" },
    { L"schedule.sat", L"السبت" },
    { L"schedule.add_quiet_hours", L"إضافة وقت هدوء" },

    // Library
    { L"library.title", L"مكتبة الأذكار" },
    { L"library.add_dhikr", L"+ إضافة ذكر" },
    { L"library.dialog_title", L"إضافة ذكر جديد" },
    { L"library.dhikr_text", L"نص الذكر" },
    { L"library.category", L"التصنيف" },
    { L"library.repeat_count", L"مرات التكرار" },
    { L"library.filter_all", L"الكل" },
    { L"library.filter_morning_evening", L"الصباح والمساء" },
    { L"library.filter_tasbih", L"التسبيح" },
    { L"library.filter_custom", L"أذكاري" },
    { L"library.delete_all", L"حذف الكل" },
    { L"library.import_md", L"استيراد ملف" },
    { L"library.empty", L"المكتبة فارغة. قم بإضافة ذكر أو استيراد ملف." },
    { L"library.delete_confirm", L"هل أنت متأكد من حذف جميع الأذكار؟" },
    { L"library.delete_title", L"تأكيد الحذف" },
    { L"library.delete_item_confirm", L"حذف هذا الذكر؟" },
    { L"library.import_success", L"تم استيراد الأذكار بنجاح!" },
    { L"library.import_error", L"فشل استيراد ملف الأذكار." },
    { L"library.items_loaded", L"عنصر متوفر" },

    // Statistics
    { L"statistics.title", L"الإحصائيات" },
    { L"statistics.today", L"اليوم" },
    { L"statistics.this_week", L"هذا الأسبوع" },
    { L"statistics.this_month", L"هذا الشهر" },
    { L"statistics.total", L"إجمالي التنبيهات" },
    { L"statistics.completed", L"مكتمل" },
    { L"statistics.streak", L"السلسلة" },
    { L"statistics.progress", L"التقدم" },
    { L"stats.daily_chart", L"النشاط اليومي" },
    { L"stats.top_dhikrs", L"أكثر الأذكار تكراراً" },

    // Popup
    { L"popup.copy", L"نسخ" },
    { L"popup.copied", L"تم النسخ!" },
    { L"popup.snooze", L"تأجيل 5د" },
    { L"popup.dismiss", L"إغلاق" },
    { L"home.next_preview_prefix", L"الذكر القادم:" },

    // Settings
    { L"settings.title", L"الإعدادات" },
    { L"settings.general", L"عام" },
    { L"settings.popup", L"النافذة المنبثقة" },
    { L"settings.sound", L"الصوت" },
    { L"settings.system", L"النظام" },
    { L"settings.language", L"اللغة" },
    { L"settings.start_with_windows", L"التشغيل مع الويندوز" },
    { L"settings.accent_color", L"لون التمييز" },
    { L"settings.reset_all", L"إعادة ضبط المصنع" },
    { L"settings.position", L"الموقع" },
    { L"settings.monitor", L"الشاشة" },
    { L"settings.duration", L"المدة" },
    { L"settings.animations", L"الرسوم المتحركة" },
    { L"settings.enable_sound", L"تفعيل الصوت" },
    { L"settings.volume", L"مستوى الصوت" },
    { L"settings.notification_sound", L"صوت التنبيه" },
    { L"settings.sound_default", L"الافتراضي (نغمة هادئة)" },
    { L"settings.choose_sound", L"استعراض ملف صوتي..." },
    { L"settings.reset_sound", L"استعادة الافتراضي" },
    { L"settings.test_sound", L"تجربة الصوت" },
    { L"settings.version", L"إصدار التطبيق" },
    { L"settings.check_updates", L"التحقق من التحديثات" },
    { L"settings.checking", L"جاري التحقق..." },
    { L"settings.up_to_date", L"التطبيق محدث" },
    { L"settings.update_available", L"يوجد تحديث متاح" },
    { L"settings.update_now", L"تحديث الآن" },
    { L"settings.reset_confirm_title", L"إعادة ضبط الإعدادات" },
    { L"settings.reset_confirm_msg", L"هل أنت متأكد من رغبتك في إعادة ضبط كافة الإعدادات إلى القيم الافتراضية؟ لا يمكن التراجع عن هذا الإجراء." },
    { L"settings.fullscreen_suppression", L"عدم المقاطعة أثناء وضع ملء الشاشة" },
    { L"settings.fullscreen_behavior", L"عند تفعيل ملء الشاشة" },
    { L"settings.fullscreen_queue", L"تأجيل التذكير حتى الخروج من ملء الشاشة" },
    { L"settings.fullscreen_suppress", L"كتم التذكير تماماً" },
    { L"settings.fullscreen_win_notify", L"إشعار ويندوز فقط" },

    // TimePicker
    { L"timepicker.title", L"تحديد الوقت" },
    { L"timepicker.apply", L"تطبيق" },
    { L"timepicker.cancel", L"إلغاء" },

    // About
    { L"about.title", L"عن التطبيق" },
    { L"about.version", L"الإصدار" },
    { L"about.contact", L"تواصل مع المطور" },
    { L"about.copy", L"نسخ" },
    { L"about.copied", L"تم النسخ!" },

    // Tray
    { L"tray.show", L"إظهار التطبيق" },
    { L"tray.pause", L"إيقاف التنبيهات مؤقتاً (ساعة)" },
    { L"tray.resume", L"استئناف التنبيهات" },
    { L"tray.settings", L"الإعدادات" },
    { L"tray.exit", L"خروج" },

    // Common
    { L"common.ok", L"موافق" },
    { L"common.cancel", L"إلغاء" },
    { L"common.close", L"إغلاق" },
    { L"common.yes", L"نعم" },
    { L"common.no", L"لا" },
    { L"common.save", L"حفظ" },
    { L"common.delete", L"حذف" },
    { L"common.warning", L"تحذير" },
    { L"common.error", L"خطأ" },
    { L"common.success", L"نجاح" },
};

Localization& Localization::Instance() {
    static Localization instance;
    return instance;
}

Localization::Localization() : m_currentLang(L"en"), m_isRtl(false) {
}

std::wstring Localization::Get(const std::wstring& key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_currentLang == L"ar") {
        auto it = g_ar.find(key);
        if (it != g_ar.end()) return it->second;
    } else {
        auto it = g_en.find(key);
        if (it != g_en.end()) return it->second;
    }
    // Fallback to key itself if not found
    return key;
}

void Localization::SetLanguage(const std::wstring& langCode) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentLang = langCode;
        m_isRtl = (m_currentLang == L"ar");
    }
    NotifyChanged();
}

std::wstring Localization::GetLanguage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_currentLang;
}

bool Localization::IsRTL() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isRtl;
}

void Localization::OnLanguageChanged(ChangeCallback cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_changeCallbacks.push_back(cb);
}

void Localization::NotifyChanged() {
    std::vector<ChangeCallback> callbacks;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        callbacks = m_changeCallbacks;
    }
    for (auto& cb : callbacks) {
        if (cb) cb();
    }
}

} // namespace dhikr
