#include "PopupWindow.h"
#include "ThemeManager.h"
#include "../../core/Scheduler.h"
#include "../../core/Database.h"
#include "../../core/Localization.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>
#include <QScreen>
#include <QClipboard>
#include <QToolTip>
#include <QEasingCurve>

namespace dhikr::qt {

PopupWindow::PopupWindow(QWidget* parent)
    : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    SetupUi();

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(m_opacityEffect);

    m_dismissTimer = new QTimer(this);
    connect(m_dismissTimer, &QTimer::timeout, this, &PopupWindow::OnTimerTick);

    connect(&ThemeManager::Instance(), &ThemeManager::ThemeChanged, this, &PopupWindow::OnThemeChanged);
}

PopupWindow::~PopupWindow() = default;

void PopupWindow::SetupUi() {
    setFixedSize(420, 240);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 16, 18, 14);
    rootLayout->setSpacing(10);

    // ── Header: Badges & Controls ──
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    m_lblCategory = new QLabel(this);
    m_lblCategory->setObjectName("CategoryBadge");
    m_lblCategory->setFont(ThemeManager::Instance().UiFont(9, true));

    m_lblSource = new QLabel(this);
    m_lblSource->setFont(ThemeManager::Instance().UiFont(9, false));
    m_lblSource->setStyleSheet("color: #94A3B8;");

    headerLayout->addWidget(m_lblCategory);
    headerLayout->addWidget(m_lblSource);
    headerLayout->addStretch();

    // Copy Button
    m_btnCopy = new QPushButton(QString::fromUtf8("📋"), this);
    m_btnCopy->setFixedSize(28, 28);
    m_btnCopy->setToolTip(QString::fromStdWString(Localization::Instance().Get(L"popup.copy", L"نسخ الأذكار")));
    connect(m_btnCopy, &QPushButton::clicked, this, &PopupWindow::OnCopyClicked);
    headerLayout->addWidget(m_btnCopy);

    // Pause Button
    m_btnPause = new QPushButton(QString::fromUtf8("⏸"), this);
    m_btnPause->setFixedSize(28, 28);
    m_btnPause->setToolTip(QString::fromStdWString(Localization::Instance().Get(L"popup.pause_15", L"إيقاف مؤقت (15 دقيقة)")));
    connect(m_btnPause, &QPushButton::clicked, this, &PopupWindow::OnPauseClicked);
    headerLayout->addWidget(m_btnPause);

    // Close Button
    m_btnClose = new QPushButton(QString::fromUtf8("✕"), this);
    m_btnClose->setFixedSize(28, 28);
    m_btnClose->setToolTip(QString::fromStdWString(Localization::Instance().Get(L"popup.close", L"إغلاق")));
    connect(m_btnClose, &QPushButton::clicked, this, &PopupWindow::Dismiss);
    headerLayout->addWidget(m_btnClose);

    rootLayout->addLayout(headerLayout);

    // ── Dhikr Arabic Text ──
    m_lblArabic = new QLabel(this);
    m_lblArabic->setObjectName("DhikrArabic");
    m_lblArabic->setFont(ThemeManager::Instance().ArabicFont(17, true));
    m_lblArabic->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_lblArabic->setWordWrap(true);
    m_lblArabic->setLayoutDirection(Qt::RightToLeft);
    rootLayout->addWidget(m_lblArabic, 1);

    // ── Transliteration / Meaning ──
    m_lblTransliteration = new QLabel(this);
    m_lblTransliteration->setObjectName("DhikrTranslation");
    m_lblTransliteration->setFont(ThemeManager::Instance().UiFont(10, false));
    m_lblTransliteration->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_lblTransliteration->setWordWrap(true);
    rootLayout->addWidget(m_lblTransliteration);

    // ── Bottom: Counter & Progress ──
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(10);

    m_btnCounter = new QPushButton(this);
    m_btnCounter->setFixedHeight(32);
    m_btnCounter->setFont(ThemeManager::Instance().UiFont(10, true));
    connect(m_btnCounter, &QPushButton::clicked, this, &PopupWindow::OnCounterClicked);
    bottomLayout->addWidget(m_btnCounter);

    m_progress = new QProgressBar(this);
    m_progress->setFixedHeight(4);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(R"(
        QProgressBar { background: #283029; border: none; border-radius: 2px; }
        QProgressBar::chunk { background: #B7F34A; border-radius: 2px; }
    )");
    bottomLayout->addWidget(m_progress, 1);

    rootLayout->addLayout(bottomLayout);
}

void PopupWindow::OnThemeChanged() {
    setStyleSheet(ThemeManager::Instance().GeneratePopupStyleSheet());
    QString primaryHex = ThemeManager::Instance().PrimaryColor().name();
    m_progress->setStyleSheet(QString(R"(
        QProgressBar { background: #283029; border: none; border-radius: 2px; }
        QProgressBar::chunk { background: %1; border-radius: 2px; }
    )").arg(primaryHex));
    update();
}

void PopupWindow::ShowReminder(const ContentItem& item) {
    m_currentItem = item;
    m_currentRepetitions = 0;
    m_targetRepetitions = (item.repetitions > 1) ? item.repetitions : 1;

    m_lblCategory->setText(QString::fromStdString(item.category));
    m_lblSource->setText(QString::fromStdWString(item.source));
    m_lblArabic->setText(QString::fromStdWString(item.text));

    if (!item.transliteration.empty()) {
        m_lblTransliteration->setText(QString::fromStdWString(item.transliteration));
        m_lblTransliteration->setVisible(true);
    } else if (!item.description.empty()) {
        m_lblTransliteration->setText(QString::fromStdWString(item.description));
        m_lblTransliteration->setVisible(true);
    } else {
        m_lblTransliteration->setVisible(false);
    }

    if (m_targetRepetitions > 1) {
        m_btnCounter->setText(QString("0 / %1").arg(m_targetRepetitions));
        m_btnCounter->setVisible(true);
    } else {
        m_btnCounter->setText(QString::fromUtf8("✓ تم القراءة"));
        m_btnCounter->setVisible(true);
    }

    OnThemeChanged();

    m_totalDurationSec = Settings::Instance().Get().popupDurationSec;
    if (m_totalDurationSec <= 0) m_totalDurationSec = 15;
    m_elapsedSec = 0;
    m_progress->setRange(0, m_totalDurationSec * 10);
    m_progress->setValue(m_totalDurationSec * 10);

    PositionWindow();
    show();
    StartEntranceAnimation();

    m_dismissTimer->start(100);

    // Record delivery in database
    Database::Instance().InsertReminderEvent(item.id, item.category, "delivered");
}

void PopupWindow::PositionWindow() {
    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect avail = screen->availableGeometry();
        int margin = 24;
        bool isRtl = Localization::Instance().IsRTL();

        int x = isRtl ? (avail.left() + margin) : (avail.right() - width() - margin);
        int y = avail.bottom() - height() - margin;
        move(x, y);
    }
}

int PopupWindow::GetAnimationDurationMs() const {
    switch (Settings::Instance().Get().animationSpeed) {
        case AnimationSpeed::Snappy: return 250;
        case AnimationSpeed::Smooth: return 600;
        case AnimationSpeed::Balanced:
        default: return 400;
    }
}

void PopupWindow::StartEntranceAnimation() {
    int duration = GetAnimationDurationMs();
    PopupAnimation animType = Settings::Instance().Get().popupAnimation;

    m_animGroup = new QParallelAnimationGroup(this);

    m_animOpacity = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_animOpacity->setDuration(duration);
    m_animOpacity->setStartValue(0.0);
    m_animOpacity->setEndValue(1.0);
    m_animOpacity->setEasingCurve(QEasingCurve::OutCubic);
    m_animGroup->addAnimation(m_animOpacity);

    QRect targetGeo = geometry();
    QRect startGeo = targetGeo;

    switch (animType) {
        case PopupAnimation::SpringBloom:
            startGeo.adjust(15, 15, -15, -15);
            m_animGeometry = new QPropertyAnimation(this, "geometry", this);
            m_animGeometry->setDuration(duration + 100);
            m_animGeometry->setStartValue(startGeo);
            m_animGeometry->setEndValue(targetGeo);
            m_animGeometry->setEasingCurve(QEasingCurve::OutBack);
            m_animGroup->addAnimation(m_animGeometry);
            break;

        case PopupAnimation::LiquidFloat:
            startGeo.translate(0, 30);
            m_animGeometry = new QPropertyAnimation(this, "geometry", this);
            m_animGeometry->setDuration(duration);
            m_animGeometry->setStartValue(startGeo);
            m_animGeometry->setEndValue(targetGeo);
            m_animGeometry->setEasingCurve(QEasingCurve::OutCubic);
            m_animGroup->addAnimation(m_animGeometry);
            break;

        case PopupAnimation::HorizontalDrift: {
            bool isRtl = Localization::Instance().IsRTL();
            startGeo.translate(isRtl ? -60 : 60, 0);
            m_animGeometry = new QPropertyAnimation(this, "geometry", this);
            m_animGeometry->setDuration(duration);
            m_animGeometry->setStartValue(startGeo);
            m_animGeometry->setEndValue(targetGeo);
            m_animGeometry->setEasingCurve(QEasingCurve::OutQuad);
            m_animGroup->addAnimation(m_animGeometry);
            break;
        }

        case PopupAnimation::RippleGenesis:
        case PopupAnimation::Origami:
        case PopupAnimation::EtherealSweep:
        case PopupAnimation::ZenBreathing:
        case PopupAnimation::Classic:
        default:
            startGeo.adjust(8, 8, -8, -8);
            m_animGeometry = new QPropertyAnimation(this, "geometry", this);
            m_animGeometry->setDuration(duration);
            m_animGeometry->setStartValue(startGeo);
            m_animGeometry->setEndValue(targetGeo);
            m_animGeometry->setEasingCurve(QEasingCurve::OutQuad);
            m_animGroup->addAnimation(m_animGeometry);
            break;
    }

    m_animGroup->start(QAbstractAnimation::DeleteWhenStopped);
}

void PopupWindow::Dismiss() {
    m_dismissTimer->stop();
    StartExitAnimation();
}

void PopupWindow::StartExitAnimation() {
    int duration = GetAnimationDurationMs() / 2;
    auto* exitAnim = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    exitAnim->setDuration(duration);
    exitAnim->setStartValue(m_opacityEffect->opacity());
    exitAnim->setEndValue(0.0);
    exitAnim->setEasingCurve(QEasingCurve::InQuad);

    connect(exitAnim, &QPropertyAnimation::finished, this, [this]() {
        hide();
        m_opacityEffect->setOpacity(1.0);
    });

    exitAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void PopupWindow::OnTimerTick() {
    if (m_isPausedByHover) return;

    m_elapsedSec++;
    int remaining = (m_totalDurationSec * 10) - m_elapsedSec;
    if (remaining <= 0) {
        Dismiss();
    } else {
        m_progress->setValue(remaining);
    }
}

void PopupWindow::OnCounterClicked() {
    m_currentRepetitions++;
    if (m_currentRepetitions >= m_targetRepetitions) {
        m_btnCounter->setText(QString::fromUtf8("✓ تم الإكمال!"));
        m_btnCounter->setStyleSheet("background-color: #B7F34A; color: #080A09; font-weight: bold;");
    } else {
        m_btnCounter->setText(QString("%1 / %2").arg(m_currentRepetitions).arg(m_targetRepetitions));
    }
}

void PopupWindow::OnCopyClicked() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(QString::fromStdWString(m_currentItem.text));
    QToolTip::showText(m_btnCopy->mapToGlobal(QPoint(0, -25)),
                       QString::fromStdWString(Localization::Instance().Get(L"popup.copied", L"تم النسخ ✓")),
                       m_btnCopy);
}

void PopupWindow::OnPauseClicked() {
    Scheduler::Instance().Pause(15);
    Dismiss();
}

void PopupWindow::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
    m_isPausedByHover = true; // Pause auto-dismiss timer while user is hovering
}

void PopupWindow::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    m_isPausedByHover = false;
}

void PopupWindow::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(1, 1, -1, -1);
    QPainterPath path;
    path.addRoundedRect(r, 16, 16);

    // Translucent dark glass background
    painter.fillPath(path, ThemeManager::Instance().PopupBackgroundColor());

    // Highlight border with subtle accent tint
    QPen pen(ThemeManager::Instance().PopupBorderColor(), 1.5);
    painter.setPen(pen);
    painter.drawPath(path);
}

} // namespace dhikr::qt
