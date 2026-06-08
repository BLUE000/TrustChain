#include "TrustChainQt.hpp"
#include <QStatusBar>
#include <QDebug>

// CMakeで注入されるマクロのデフォルトフォールバック
#ifndef TRUSTCHAIN_BUILD_IS_CUSTOMIZED
#define TRUSTCHAIN_BUILD_IS_CUSTOMIZED 1
#endif

#ifndef TRUSTCHAIN_CREATOR_NAME
#define TRUSTCHAIN_CREATOR_NAME "BLUE000"
#endif

#ifndef TRUSTCHAIN_VERSION
#define TRUSTCHAIN_VERSION "Unknown"
#endif

namespace TrustChain {

void QtHelper::applyWatermark(QMainWindow* window, AuthStatus status)
{
    if (!window) return;

    // 1. 強制終了ステータスの場合は即座にアプリを終了
    if (status == AuthStatus::Terminated) {
        Core::terminateApplication("このビルドはサーバー側で無効化されているため、起動できません。");
        return;
    }

    // 2. 非公式判定、オフライン、または通信エラー時のウォーターマーク適用
    if (status == AuthStatus::Watermarked) {
        qInfo("[TrustChain] Applying origin watermark (Custom Build mode) to Main Window.");

        // ① タイトルバーの自動書き換え
        QString currentTitle = window->windowTitle();
        const QString customTag = " (Custom Build)";
        if (!currentTitle.contains(customTag)) {
            window->setWindowTitle(currentTitle + customTag);
        }

        // ② ステータスバーの強制保護とコピーライト表示
        QStatusBar* statusBar = window->statusBar(); // なければ自動生成される
        if (statusBar) {
            QString copyrightOwner = QString::fromUtf8(TRUSTCHAIN_CREATOR_NAME);
            QString version = QString::fromUtf8(TRUSTCHAIN_VERSION);
            statusBar->showMessage(QString("© %1 (Original Creator) v%2").arg(copyrightOwner, version));

            // 他のテキストで上書きや、非表示化されるのをCSSと属性で保護
            statusBar->setStyleSheet(
                "color: #888888; "
                "background-color: #121214; "
                "font-weight: bold; "
                "border-top: 1px solid #2d2d30;"
            );
            
            // ステータスバーを表示状態にロック
            statusBar->setVisible(true);
        }
    } else {
        qInfo("[TrustChain] Official build authorized. Normal window loading.");
    }
}

} // namespace TrustChain
