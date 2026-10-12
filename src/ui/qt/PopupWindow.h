#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QParallelAnimationGroup>
#include "../../core/ContentEngine.h"
#include "../../core/Settings.h"

namespace dhikr::qt {

class PopupWindow : public QWidget {
    Q_OBJECT

public:
    explicit PopupWindow(QWidget* parent = nullptr);
    ~PopupWindow() override;

    // Show reminder popup with specified content item
    void ShowReminder(const ContentItem& item);

    // Dismiss with exit animation
    void Dismiss();

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void OnTimerTick();
    void OnCounterClicked();
    void OnCopyClicked();
    void OnPauseClicked();
    void OnThemeChanged();

private:
    void SetupUi();
    void PositionWindow();
    void StartEntranceAnimation();
    void StartExitAnimation();
    int GetAnimationDurationMs() const;

    // UI elements
    QLabel*       m_lblCategory = nullptr;
    QLabel*       m_lblSource = nullptr;
    QLabel*       m_lblArabic = nullptr;
    QLabel*       m_lblTransliteration = nullptr;
    QLabel*       m_lblDescription = nullptr;
    QPushButton*  m_btnCounter = nullptr;
    QPushButton*  m_btnCopy = nullptr;
    QPushButton*  m_btnPause = nullptr;
    QPushButton*  m_btnClose = nullptr;
    QProgressBar* m_progress = nullptr;

    // Animation & State
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QPropertyAnimation*     m_animOpacity = nullptr;
    QPropertyAnimation*     m_animGeometry = nullptr;
    QParallelAnimationGroup* m_animGroup = nullptr;

    QTimer* m_dismissTimer = nullptr;
    int     m_elapsedSec = 0;
    int     m_totalDurationSec = 15;
    bool    m_isPausedByHover = false;

    // Current item data
    ContentItem m_currentItem;
    int         m_currentRepetitions = 0;
    int         m_targetRepetitions = 1;
};

} // namespace dhikr::qt
