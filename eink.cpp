#include "eink.h"

#include "diagnostics.h"

#include <inkview.h>
#include <QGuiApplication>
#include <QQuickWindow>
#include <common_utilities/einkinterface.h>

using pocketbook::utilities::einkPlatform;

void Eink::enterA2ForFlick(int x, int y, int width, int height)
{
    // nullptr until the platform plugin has started
    if (auto *platform = einkPlatform()) {
        // auto_exit would leave A2 as soon as the finger lifts, while the list is still
        // coasting; leaveA2() ends it when the movement has really stopped
        platform->a2ForFlick({x, y, width, height, 0}, false);
        diagnostics::log(QString("A2 on for %1,%2 %3x%4").arg(x).arg(y).arg(width).arg(height));
    }
}

void Eink::leaveA2()
{
    if (auto *platform = einkPlatform(); platform && platform->isInA2()) {
        platform->leaveA2();
        diagnostics::log("A2 off");
    }
}

void Eink::setDynamicUpdates(bool enabled)
{
    auto *platform = einkPlatform();
    if (!platform || enabled == m_dynamicUpdates)
        return;
    m_dynamicUpdates = enabled;

    // Enabled, not EnabledBW: on colour screens EnabledBW adds artifacts to anti-aliased
    // text and 1px lines, and the text fields have both
    using Mode = pocketbook::utilities::EinkPlatformInterface::DynamicUpdateMode;
    platform->setDynamicUpdateMode(enabled ? Mode::Enabled : Mode::Disabled);
    diagnostics::log(enabled ? "dynamic updates on" : "dynamic updates off");

    // Redraw at normal quality, clearing any artifacts left from typing
    if (!enabled) {
        for (QWindow *window : QGuiApplication::topLevelWindows()) {
            if (auto *quickWindow = qobject_cast<QQuickWindow *>(window))
                quickWindow->update();
        }
    }
}
