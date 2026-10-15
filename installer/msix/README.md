# Azkar Reminder - Microsoft Store Package (MSIX)

حزمة التثبيت الرسمية لمتجر مايكروسوفت (Microsoft Store) و Windows 10 / Windows 11.

---

## 📦 الملفات الجاهزة في هذا المجلد

- **`AzkarReminder-1.2.4.msix`**: ملف الحزمة الجاهز للرفع على Microsoft Partner Center أو للتثبيت المحلي (~13.4 ميجابايت).
- **`AzkarReminder-DeveloperCert.cer`**: الشهادة الرقمية المخصصة لتثبيت الحزمة محلياً (Sideloading) بدون متجر.
- **`AppxManifest.xml`**: ملف المانيفست الرسمي للتطبيق متضمن الهوية والصلاحيات والأيقونات.
- **`build_msix.ps1`**: سكربت بناء الحزمة التلقائي عبر PowerShell وأدوات Windows SDK.
- **`create_assets.ps1`**: سكربت توليد أيقونات المتجر بجميع المقاسات من الشعار الأصلي.
- **`Assets/`**: مجلد الأيقونات والشاشات المجهزة بمقاسات المتجر.

---

## 🏪 طريقة رفع الحزمة على متجر مايكروسوفت (Microsoft Store)

1. افتح لوحة التحكم الخاصة بالمطورين: [Microsoft Partner Center](https://partner.microsoft.com/dashboard).
2. اختر **Windows & Xbox** ثم اضغط على **Create a new app**.
3. احجز اسم التطبيق: `Azkar Reminder`.
4. ابدأ إرسال جديد (**Start your submission**)، وتوجه إلى قسم **Packages**.
5. قم بسحب وإسقاط ملف:
   ```
   AzkarReminder-1.2.4.msix
   ```
6. سيقوم نظام مايكروسوفت بالتحقق من الحزمة وفحصها تلقائياً وتوقيعها بشهادة المتجر الرسمية.
7. أكمل وصف التطبيق والصور واضغط **Submit to the Store**.

---

## 💻 طريقة التثبيت والتجربة المباشرة على ويندوز (Sideloading)

إذا أردت تثبيت الحزمة على جهازك مباشرة بدون رفعها للمتجر:

1. اضغط مرتين على ملف الشهادة: `AzkarReminder-DeveloperCert.cer`.
2. اضغط **Install Certificate...**
3. اختر **Local Machine** واضغط **Next**.
4. اختر **Place all certificates in the following store** ثم اضغط **Browse...** واختر:
   - **`Trusted People`** (أو "الأشخاص الموثوق بهم").
5. اضغط **Next** ثم **Finish**.
6. الآن اضغط مرتين على ملف `AzkarReminder-1.2.4.msix` واضغط **Install**؛ سيتم تثبيت التطبيق ويعمل مباشرة!
