#include "TrustChainQt.hpp"
#include <QStatusBar>
#include <QDebug>
#include <QFile>
#include <QCoreApplication>
#include <QDir>

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

struct WatermarkData {
    QString copyright;
    QString version;
    bool found = false;
};

// バイナリ末尾のオーバーレイから [BM_START] ~ [BM_END] タグを探索し、出自情報を抽出
static WatermarkData extractWatermarkFromOverlay() {
    WatermarkData result;
    QString appFilePath = QCoreApplication::applicationFilePath();
    QFile file(appFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return result;
    }
    
    qint64 fileSize = file.size();
    // オーバーレイ領域はファイル末尾に存在するため、末尾64KBを読み取る
    qint64 readSize = qMin(fileSize, static_cast<qint64>(65536));
    if (readSize > 0) {
        if (file.seek(fileSize - readSize)) {
            QByteArray bytes = file.read(readSize);
            
            int startIdx = bytes.indexOf("[BM_START]");
            int endIdx = bytes.indexOf("[BM_END]");
            if (startIdx != -1 && endIdx != -1 && endIdx > startIdx) {
                QByteArray block = bytes.mid(startIdx + 10, endIdx - (startIdx + 10));
                result.found = true;
                
                QList<QByteArray> lines = block.split('\n');
                for (const QByteArray& line : lines) {
                    QString lineStr = QString::fromUtf8(line.trimmed());
                    if (lineStr.startsWith("Copyright:", Qt::CaseInsensitive)) {
                        result.copyright = lineStr.mid(10).trimmed();
                    } else if (lineStr.startsWith("Version:", Qt::CaseInsensitive)) {
                        result.version = lineStr.mid(8).trimmed();
                    } else if (lineStr.contains("Copyright", Qt::CaseInsensitive) && result.copyright.isEmpty()) {
                        result.copyright = lineStr;
                    }
                }
            }
        }
    }
    file.close();
    return result;
}

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

        // バイナリ透かし (Overlay Watermark) の抽出
        WatermarkData wm = extractWatermarkFromOverlay();

        QString copyrightOwner;
        QString version;
        if (wm.found && !wm.copyright.isEmpty()) {
            copyrightOwner = wm.copyright;
            version = wm.version.isEmpty() ? QString::fromUtf8(TRUSTCHAIN_VERSION) : wm.version;
        } else {
            // 埋め込みデータが未設定、または開発中ビルドの場合のフォールバック値
            copyrightOwner = QString("© %1 (Original Creator)").arg(QString::fromUtf8(TRUSTCHAIN_CREATOR_NAME));
            version = QString::fromUtf8(TRUSTCHAIN_VERSION);
        }

        // ① タイトルバーの自動書き換え
        QString currentTitle = window->windowTitle();
        QString customTag = QString(" (Custom Build: %1 v%2)").arg(copyrightOwner, version);
        if (!currentTitle.contains("Custom Build")) {
            window->setWindowTitle(currentTitle + customTag);
        }

        // ② ステータスバーの強制保護とコピーライト表示
        QStatusBar* statusBar = window->statusBar(); // なければ自動生成される
        if (statusBar) {
            statusBar->showMessage(QString("%1 v%2").arg(copyrightOwner, version));

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
