#include "ui/eink.h"

#include "core/diagnostics.h"

#include <inkview.h>
#include <QGuiApplication>
#include <QQuickWindow>
#include <common_utilities/einkinterface.h>

using pocketbook::utilities::einkPlatform;

// How long A2 stays on after a list stops. The last frames of a flick are still drawing
// when movement ends, so leaving at once would redraw at full quality and then paint those
// frames over it. The system apps also wait before redrawing.
static constexpr int a2SettleMs = 1000;

void Eink::followNightMode()
{
    IvSetAppCapability(IvGetAppCapability() | APP_CAPABILITY_SUPPORT_SCREEN_INVERSION);
    m_nightMode = IvGetScreenModeInversion();
    diagnostics::log(m_nightMode ? "night mode on" : "night mode off");

    auto *platform = einkPlatform();
    if (!platform)
        return;
    platform->addHandlerForInkvewEvents([this](int type, int, int) {
        if (type != EVT_SCREEN_INVERSION_MODE_CHANGED)
            return;
        QMetaObject::invokeMethod(this, [this] {
            // The change can arrive twice, and every full update flashes
            const bool nightMode = IvGetScreenModeInversion();
            if (nightMode == m_nightMode)
                return;
            m_nightMode = nightMode;
            diagnostics::trace(m_nightMode ? "night mode on" : "night mode off");
            emit nightModeChanged();
            // Most of the window is unchanged, so only a full update redraws all of it
            if (auto *platform = einkPlatform())
                platform->requireFullUpdate();
            for (QWindow *window : QGuiApplication::topLevelWindows()) {
                if (auto *quickWindow = qobject_cast<QQuickWindow *>(window))
                    quickWindow->update();
            }
        }, Qt::QueuedConnection);
    });
}

void Eink::enterA2ForFlick(int x, int y, int width, int height)
{
    // nullptr until the platform plugin has started
    if (auto *platform = einkPlatform()) {
        // Moving again before the timed exit: stay in the current A2
        platform->cancelLeaveA2Timer();
        if (platform->isInA2())
            return;
        // auto_exit would leave A2 when the finger lifts while the list is still coasting.
        // leaveA2() ends it when movement has really stopped.
        platform->a2ForFlick({x, y, width, height, 0}, false);
        diagnostics::trace(QString("A2 on for %1,%2 %3x%4").arg(x).arg(y).arg(width).arg(height));
    }
}

void Eink::leaveA2Soon()
{
    if (auto *platform = einkPlatform(); platform && platform->isInA2()) {
        platform->timedLeaveA2(a2SettleMs);
        diagnostics::trace(QString("A2 off in %1 ms").arg(a2SettleMs));
    }
}

void Eink::leaveA2()
{
    if (auto *platform = einkPlatform(); platform && platform->isInA2()) {
        platform->cancelLeaveA2Timer();
        platform->leaveA2();
        diagnostics::trace("A2 off");
    }
}

void Eink::setDynamicUpdates(bool enabled)
{
    auto *platform = einkPlatform();
    if (!platform || enabled == m_dynamicUpdates)
        return;
    m_dynamicUpdates = enabled;

    // Enabled, not EnabledBW: on colour screens EnabledBW leaves artifacts on anti-aliased
    // text and 1px lines, which the text fields have
    using Mode = pocketbook::utilities::EinkPlatformInterface::DynamicUpdateMode;
    platform->setDynamicUpdateMode(enabled ? Mode::Enabled : Mode::Disabled);
    diagnostics::trace(enabled ? "dynamic updates on" : "dynamic updates off");

    // Redraw at normal quality, clearing any artifacts from typing
    if (!enabled) {
        for (QWindow *window : QGuiApplication::topLevelWindows()) {
            if (auto *quickWindow = qobject_cast<QQuickWindow *>(window))
                quickWindow->update();
        }
    }
}
