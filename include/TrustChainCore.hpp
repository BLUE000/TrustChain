#pragma once

#include <QString>

class QNetworkAccessManager;

namespace TrustChain {

/**
 * @brief 認証およびビルド出自の検証結果ステータス
 */
enum class AuthStatus {
    Normal,         ///< 公式ビルドかつ認証成功 (通常起動)
    Watermarked,    ///< 非公式、オフライン、または通信エラー (ウォーターマーク表示)
    Terminated      ///< トークンがサーバー側で無効化された (起動不可・強制終了)
};

/**
 * @brief 起動時のオンライン出自認証を行うコアロジッククラス
 * 
 * GUIに依存せず、QtCore および QtNetwork のみを用いて動作します。
 */
class Core {
public:
    Core();

    /**
     * @brief 埋め込まれたトークンをオンライン検証します（同期的/ブロッキング実行）
     * 
     * 内部で QEventLoop とタイマーを使用し、最大3.0秒でタイムアウトします。
     * トークン内の \0 (ヌル文字) や不正制御文字の検知処理を含みます。
     * 
     * @return AuthStatus 判定されたステータス
     */
    AuthStatus verifyToken();

    /**
     * @brief テスト用のネットワークマネージャーを設定します（依存性注入）
     * @param manager テスト用モックマネージャー
     */
    void setNetworkAccessManager(QNetworkAccessManager* manager);

    /**
     * @brief トークン無効化が検知された際に、ダイアログ表示とプロセスの強制シャットダウンを行います
     * 
     * @param errorMessage ダイアログやログに出力するカスタムエラーメッセージ
     */
    static void terminateApplication(const QString& errorMessage = QString());

    /**
     * @brief ビルドの出自状態（非公式フラグ）を設定します（テスト・デバッグ用）
     */
    void setBuildIsCustomized(bool isCustomized);

    /**
     * @brief ビルドの出自状態（非公式フラグ）を取得します
     */
    bool isBuildCustomized() const;

    /**
     * @brief トークン文字列に \0 (ヌル文字) 等の危険な制御文字が含まれていないか厳密に検証します
     * @param token 検証対象의 トークン
     * @return true 安全なトークンである場合
     * @return false 汚染・改ざんの疑いがある場合
     */
    bool validateTokenSecurity(const QString& token) const;

private:
    QString m_apiUrl;
    QString m_apiToken;
    bool m_buildIsCustomized;
    QNetworkAccessManager* m_customManager = nullptr;
};

} // namespace TrustChain
