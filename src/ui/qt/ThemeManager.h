#pragma once

#include <QObject>
#include <QColor>
#include <QFont>
#include <QLinearGradient>
#include <QString>
#include "../../core/Settings.h"

namespace dhikr::qt {

class ThemeManager : public QObject {
    Q_OBJECT

public:
    static ThemeManager& Instance();

    // Primary accent color based on current active theme or paused state
    QColor PrimaryColor(qreal alpha = 1.0) const;

    // Dark / secondary accent color
    QColor PrimaryDarkColor(qreal alpha = 1.0) const;

    // Background and surface colors
    QColor BackgroundColor() const;
    QColor SidebarColor() const;
    QColor SurfaceColor() const;
    QColor CardColor() const;
    QColor CardHoverColor() const;
    QColor BorderColor() const;
    QColor BorderSubtleColor() const;

    // Text colors
    QColor TextPrimaryColor() const;
    QColor TextSecondaryColor() const;
    QColor TextMutedColor() const;

    // Popup reminder colors
    QColor PopupBackgroundColor() const;
    QColor PopupBorderColor() const;

    // Status checks
    bool IsAppPaused() const;
    ColorTheme CurrentTheme() const;

    // Styling
    QString GenerateApplicationStyleSheet() const;
    QString GeneratePopupStyleSheet() const;
    QFont ArabicFont(int pointSize = 14, bool bold = false) const;
    QFont UiFont(int pointSize = 10, bool bold = false) const;

public slots:
    void RefreshTheme();

signals:
    void ThemeChanged();

private:
    ThemeManager();
    ~ThemeManager() override = default;

    QColor GetThemePrimary(ColorTheme theme, qreal alpha = 1.0) const;
};

} // namespace dhikr::qt
