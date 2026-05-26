#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMap>
#include <QUrl>
#include <QByteArray>
#include <QTimer>

/**
 * @brief テスト用のモック QNetworkReply クラス
 */
class MockNetworkReply : public QNetworkReply {
public:
    MockNetworkReply(const QUrl& url, int statusCode, const QByteArray& responseData, int delayMs = 50, QObject* parent = nullptr)
        : QNetworkReply(parent)
        , m_data(responseData)
        , m_offset(0)
    {
        setUrl(url);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, statusCode);
        
        // 読み込み専用でオープン
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);
        
        // 指定された遅延時間（ミリ秒）後に非同期的に完了シグナルを発火させる
        QTimer::singleShot(delayMs, this, [this]() {
            if (error() == QNetworkReply::NoError) {
                emit readyRead();
            }
            emit finished();
        });
    }

    void abort() override {
        setError(QNetworkReply::OperationCanceledError, "Operation aborted");
        emit finished();
    }

protected:
    qint64 readData(char* data, qint64 maxlen) override {
        if (m_offset >= m_data.size()) return 0;
        qint64 len = qMin(maxlen, static_cast<qint64>(m_data.size() - m_offset));
        memcpy(data, m_data.constData() + m_offset, len);
        m_offset += len;
        return len;
    }

    qint64 writeData(const char*, qint64) override { return 0; }

private:
    QByteArray m_data;
    int m_offset;
};

/**
 * @brief テスト用のモック QNetworkAccessManager クラス
 * 
 * 登録されたURLに対して事前に指定されたステータスコードとJSON応答をエミュレートします。
 */
class MockNetworkAccessManager : public QNetworkAccessManager {
public:
    struct MockResponse {
        int statusCode;
        QByteArray data;
    };

    /**
     * @brief 期待されるレスポンスを事前登録します
     */
    void setExpectedResponse(const QUrl& url, int statusCode, const QByteArray& data) {
        m_expectedResponses[url] = {statusCode, data};
    }

    /**
     * @brief 遅延時間（ミリ秒）を設定してタイムアウトテストを模擬します
     */
    void setResponseDelay(int delayMs) {
        m_delayMs = delayMs;
    }

protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& req, QIODevice* outgoingData) override {
        Q_UNUSED(op);
        Q_UNUSED(outgoingData);

        QUrl url = req.url();
        MockResponse resp = { 404, "{\"status\":\"error\",\"message\":\"Mock Not Found\"}" };

        if (m_expectedResponses.contains(url)) {
            resp = m_expectedResponses[url];
        }

        // 遅延時間を適用してモックレスポンスを生成
        int delay = m_delayMs > 0 ? m_delayMs : 50;
        return new MockNetworkReply(url, resp.statusCode, resp.data, delay, this);
    }

private:
    QMap<QUrl, MockResponse> m_expectedResponses;
    int m_delayMs = 0;
};
