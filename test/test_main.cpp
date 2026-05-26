#include <gtest/gtest.h>
#include <QApplication>
#include <iostream>

// Windows環境下で qFatal がポップアップダイアログを表示してテストをブロックするのを防ぐカスタムハンドラ
void quietMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s\n", localMsg.constData());
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s\n", localMsg.constData());
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s\n", localMsg.constData());
        std::exit(-1); // ダイアログを出さずに即時終了
    }
}

int main(int argc, char* argv[])
{
    // メッセージハンドラをインストールして、クラッシュダイアログの表示を抑止する
    qInstallMessageHandler(quietMessageHandler);

    // Qt のイベントループやタイマーを使用するため、GTest実行前に QApplication を初期化します。
    QApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
