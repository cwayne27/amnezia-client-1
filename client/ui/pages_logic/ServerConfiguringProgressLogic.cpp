#include "ServerConfiguringProgressLogic.h"
#include "defines.h"
#include "core/errorstrings.h"
#include <QTimer>
#include <QEventLoop>
#include <QMessageBox>

#include "core/servercontroller.h"

ServerConfiguringProgressLogic::ServerConfiguringProgressLogic(UiLogic *logic, QObject *parent):
    PageLogicBase(logic, parent),
    m_progressBarValue{0},
    m_labelWaitInfoVisible{true},
    m_labelWaitInfoText{tr("Please wait, configuring process may take up to 5 minutes")},
    m_progressBarVisible{true},
    m_progressBarMaximium{100},
    m_progressBarTextVisible{true},
    m_progressBarText{tr("Configuring...")},
    m_labelServerBusyVisible{false},
    m_labelServerBusyText{""}
{

}

void ServerConfiguringProgressLogic::onUpdatePage()
{
    set_progressBarValue(0);
}


ErrorCode ServerConfiguringProgressLogic::doInstallAction(const std::function<ErrorCode()> &action)
{
    PageFunc page;
    page.setEnabledFunc = [this] (bool enabled) -> void {
        set_pageEnabled(enabled);
    };
    ButtonFunc noButton;
    LabelFunc noWaitInfo;
    ProgressFunc progress;
    progress.setVisibleFunc = [this] (bool visible) -> void {
        set_progressBarVisible(visible);
    };

    progress.setValueFunc = [this] (int value) -> void {
        set_progressBarValue(value);
    };
    progress.getValueFunc = [this] (void) -> int {
        return progressBarValue();
    };
    progress.getMaximiumFunc = [this] (void) -> int {
        return progressBarMaximium();
    };

    LabelFunc busyInfo;
    busyInfo.setTextFunc = [this] (const QString& text) -> void {
        set_labelServerBusyText(text);
    };
    busyInfo.setVisibleFunc = [this] (bool visible) -> void {
        set_labelServerBusyVisible(visible);
    };

    return doInstallAction(action, page, progress, noButton, noWaitInfo, busyInfo);
}

ErrorCode ServerConfiguringProgressLogic::doInstallAction(const std::function<ErrorCode()> &action,
                                                          const PageFunc &page,
                                                          const ProgressFunc &progress,
                                                          const ButtonFunc &button,
                                                          const LabelFunc &waitInfo,
                                                          const LabelFunc &serverBusyInfo)
{
    progress.setVisibleFunc(true);
    if (page.setEnabledFunc) {
        page.setEnabledFunc(false);
    }
    if (button.setVisibleFunc) {
        button.setVisibleFunc(false);
    }
    if (waitInfo.setVisibleFunc) {
        waitInfo.setVisibleFunc(true);
    }
    if (waitInfo.setTextFunc) {
        waitInfo.setTextFunc(tr("Please wait, configuring process may take up to 5 minutes"));
    }

    QTimer timer;
    connect(&timer, &QTimer::timeout, [progress](){
        progress.setValueFunc(progress.getValueFunc() + 1);
    });

    progress.setValueFunc(0);
    timer.start(1000);

    QMetaObject::Connection connection;
    if (serverBusyInfo.setTextFunc) {
        connection = connect(m_serverController.get(),
                             &ServerController::serverIsBusy,
                             this,
                             [&serverBusyInfo, &timer](const bool isBusy) {
            isBusy ? timer.stop() : timer.start(1000);
            serverBusyInfo.setVisibleFunc(isBusy);
            serverBusyInfo.setTextFunc(isBusy ? "Amnesia has detected that your server is currently "
                                                "busy installing other software. Amnesia installation "
                                                "will pause until the server finishes installing other software"
                                              : "");
        });
    }

    ErrorCode e = action();
    qDebug() << "doInstallAction finished with code" << e;
    if (serverBusyInfo.setTextFunc) {
        disconnect(connection);
    }

    if (e) {
        if (page.setEnabledFunc) {
            page.setEnabledFunc(true);
        }
        if (button.setVisibleFunc) {
            button.setVisibleFunc(true);
        }
        if (waitInfo.setVisibleFunc) {
            waitInfo.setVisibleFunc(false);
        }
        QMessageBox::warning(nullptr, APPLICATION_NAME,
                             tr("Error occurred while configuring server.") + "\n" +
                             errorString(e));

        progress.setVisibleFunc(false);
        return e;
    }

    // just ui progressbar tweak
    timer.stop();

    int remainingVal = progress.getMaximiumFunc() - progress.getValueFunc();

    if (remainingVal > 0) {
        QTimer timer1;
        QEventLoop loop1;

        connect(&timer1, &QTimer::timeout, [&](){
            progress.setValueFunc(progress.getValueFunc() + 1);
            if (progress.getValueFunc() >= progress.getMaximiumFunc()) {
                loop1.quit();
            }
        });

        timer1.start(5);
        loop1.exec();
    }


    progress.setVisibleFunc(false);
    if (button.setVisibleFunc) {
        button.setVisibleFunc(true);
    }
    if (page.setEnabledFunc) {
        page.setEnabledFunc(true);
    }
    if (waitInfo.setTextFunc) {
        waitInfo.setTextFunc(tr("Operation finished"));
    }
    return ErrorCode::NoError;
}
