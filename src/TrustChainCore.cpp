#include "TrustChainCore.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>
#include <iostream>

// CMakeで注入されるマクロのデフォルトフォールバック
#ifndef TRUSTCHAIN_BUILD_IS_CUSTOMIZED
#define TRUSTCHAIN_BUILD_IS_CUSTOMIZED 1 ///< 定義なしは安全側に倒して非公式扱い
#endif

#ifndef TRUSTCHAIN_API_TOKEN
#define TRUSTCHAIN_API_TOKEN "UNAUTHORIZED_TOKEN"
#endif

#ifndef TRUSTCHAIN_CREATOR_NAME
#define TRUSTCHAIN_CREATOR_NAME "BLUE000"
#endif

#ifndef TRUSTCHAIN_TOKEN_ISSUER_URL
#define TRUSTCHAIN_TOKEN_ISSUER_URL "https://streamers-tool.sakura.ne.jp/TransCipher/index.php"
#endif

namespace TrustChain {

Core::Core()
    : m_apiUrl(TRUSTCHAIN_TOKEN_ISSUER_URL) // credentialsファイルまたはフォールバックから読み込み
    , m_apiToken(QString::fromUtf8(TRUSTCHAIN_API_TOKEN))
    , m_buildIsCustomized(TRUSTCHAIN_BUILD_IS_CUSTOMIZED == 1)
{
    // コンストラクタ時点でトークンから \0 などをトリミングして格納
    m_apiToken = m_apiToken.trimmed();
}

AuthStatus Core::verifyToken()
{
    // 1. ビルド時に「非公式/出自不一致」と判定されている場合は、オンライン検証をスキップしてウォーターマークへ
    if (m_buildIsCustomized) {
        qInfo("[TrustChain] Custom build detected by macro. Skipping online verification.");
        return AuthStatus::Watermarked;
    }

    // 2. トークン内の \0 (ヌル文字) 等の危険な汚染チェック
    if (!validateTokenSecurity(m_apiToken)) {
        qWarning("[TrustChain] Token verification bypassed due to invalid/contaminated token string.");
        return AuthStatus::Watermarked;
    }

    // 3. オンライン検証の実行 (QNetworkAccessManagerを使用)
    QNetworkAccessManager localManager;
    QNetworkAccessManager* manager = m_customManager ? m_customManager : &localManager;
    
    QJsonObject payload;
    payload["action"] = "check_status";
    payload["token"] = m_apiToken; // トリミング済みの安全なトークンを設定
    
    QJsonDocument doc(payload);
    QByteArray postData = doc.toJson(QJsonDocument::Compact);

    // QNetworkRequest の作成
    QNetworkRequest request;
    request.setUrl(QUrl(m_apiUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // トークン内のヌル終端の誤認を防ぐため、Content-Lengthをバイトサイズで厳密に設定
    request.setHeader(QNetworkRequest::ContentLengthHeader, QByteArray::number(postData.size()));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    qInfo("[TrustChain] Initiating online provenance verification with server...");
    QNetworkReply* reply = manager->post(request, postData);

    // 非同期通信を同期的（ブロッキング）に処理するためのイベントループバインド
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        qWarning("[TrustChain] Online verification timed out (3.0s).");
        reply->abort();
        loop.quit();
    });

    timer.start(3000); // 3.0秒でタイムアウト判定
    loop.exec();       // 通信完了またはタイムアウトまでブロッキング

    // タイムアウトまたはネットワーク切断の場合のハンドリング
    if (reply->error() != QNetworkReply::NoError) {
        qWarning("[TrustChain] Network error during verification: %s (Error code: %d)", 
                 qUtf8Printable(reply->errorString()), reply->error());
        reply->deleteLater();
        return AuthStatus::Watermarked; // 安全側に倒してウォーターマーク表示で起動
    }

    // レスポンスJSONの解析
    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    // パース段階でのヌル文字・バイナリ破壊対策（Qt標準パーサーによる安全な検証）
    QJsonParseError parseError;
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError || responseDoc.isNull() || !responseDoc.isObject()) {
        qWarning("[TrustChain] Invalid JSON response received from verification server: %s", 
                 qUtf8Printable(parseError.errorString()));
        return AuthStatus::Watermarked;
    }

    QJsonObject responseObj = responseDoc.object();
    QString status = responseObj.value("status").toString().trimmed();
    QString tokenStatus = responseObj.value("token_status").toString().trimmed();

    if (status == "success" && tokenStatus == "active") {
        qInfo("[TrustChain] Provenance verification successful. Standard official build authorized.");
        return AuthStatus::Normal;
    } else if (status == "success" && tokenStatus == "inactive") {
        // サーバーが明確に "token_status: inactive" を返した場合は、該当トークンが無効化（ブラックリスト入り）されているため強制終了
        qCritical("[TrustChain] Token has been explicitly revoked or invalidated by administrator.");
        return AuthStatus::Terminated;
    }

    // 想定外のレスポンスの場合は安全のためウォーターマーク表示ルートへ
    qWarning("[TrustChain] Server returned unexpected response status: %s", qUtf8Printable(status));
    return AuthStatus::Watermarked;
}

void Core::setNetworkAccessManager(QNetworkAccessManager* manager)
{
    m_customManager = manager;
}

void Core::setBuildIsCustomized(bool isCustomized)
{
    m_buildIsCustomized = isCustomized;
}

bool Core::isBuildCustomized() const
{
    return m_buildIsCustomized;
}

bool Core::validateTokenSecurity(const QString& token) const
{
    // 1. 空のトークンは不正判定
    if (token.isEmpty() || token == "UNAUTHORIZED_TOKEN") {
        return false;
    }

    // 2. 直接のヌル文字 (\0) 混入チェック
    if (token.contains(QChar('\0')) || token.contains('\0')) {
        qWarning("[TrustChain] SECURITY ALERT: Null character (\\0) detected in API token! Rejected.");
        return false;
    }

    // 3. UTF-8にシリアライズした際のヌルバイト混入チェック
    QByteArray utf8Data = token.toUtf8();
    if (utf8Data.contains('\0')) {
        qWarning("[TrustChain] SECURITY ALERT: Null byte in UTF-8 representation of token! Rejected.");
        return false;
    }

    // 4. 文字コード検査による制御文字の混入チェック（ASCII 0x00 ~ 0x1F, 0x7F）
    // 前後の空白や通常の改行は .trimmed() でコンストラクタ時に取り除くため、
    // トークン内部に制御文字が混ざっている場合は異常（攻撃や破損）とみなす
    for (const QChar& ch : token) {
        if (ch.isSpace()) continue; // スペース、標準タブ、標準改行は除外
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7F) {
            qWarning("[TrustChain] SECURITY ALERT: Invisible control character (0x%02X) detected in token! Rejected.", 
                     ch.unicode());
            return false;
        }
    }

    return true;
}

void Core::terminateApplication(const QString& errorMessage)
{
    QString displayMsg = errorMessage.isEmpty()
        ? "セキュリティエラー: このアプリケーションビルドは無効化されているため、起動できません。"
        : errorMessage;

    // 標準エラーとデバッグログに出力
    std::cerr << "[TrustChain] TERMINATED: " << displayMsg.toStdString() << std::endl;
    
    // Windows環境において、ダイアログ表示と安全なプロセスアボートを行うために qFatal を起動
    qFatal("%s", qUtf8Printable(displayMsg));
    
    std::exit(-1);
}

} // namespace TrustChain
