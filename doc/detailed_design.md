# TrustChain 詳細設計書 (Detailed Design)

本ドキュメントは、C++セキュリティフレームワーク「**TrustChain**」のコンポーネント、モジュール、インターフェースに関する詳細設計書です。

---

## 1. 外部設定ファイル仕様 (`trustchain_credentials.cmake`)

開発環境ごとの環境情報や、リポジトリにコミットしてはならない機密情報を格納するファイルの仕様です。

### 1.1. ファイル配置とGit管理
* **配置場所**: 導入先プロジェクトのルートディレクトリ、または `CMakeLists.txt` と同一ディレクトリ。
* **Git管理除外**: 導入先プロジェクトの `.gitignore` に必ず以下を追記する。
  ```
  # TrustChain credentials file
  /trustchain_credentials.cmake
  ```

### 1.2. ファイルフォーマット (`trustchain_credentials.cmake`)

```cmake
# =========================================================================
# TrustChain 認証・シークレット設定ファイル (Git管理外・漏洩厳禁)
# =========================================================================

# 1. 接続先リポジトリ情報
set(TRUSTCHAIN_GITHUB_USER "BLUE000" CACHE STRING "GitHub username or organization")
set(TRUSTCHAIN_GITHUB_REPO "TwitchFollowerChecker" CACHE STRING "GitHub repository name")
set(TRUSTCHAIN_TARGET_BRANCH "main" CACHE STRING "Target branch to check tampering (main/master)")

# 2. TransCipher Web API 接続情報
set(TRUSTCHAIN_TOKEN_ISSUER_URL "https://streamers-tool.sakura.ne.jp/TransCipher/index.php" CACHE STRING "URL for automated token issuance")
set(TRUSTCHAIN_BUILD_SECRET "BLUE000_BUILD_SECRET_KEY" CACHE STRING "Secret key for build system to request tokens")

# 3. コピーライト（ウォーターマーク）文言の初期値
set(TRUSTCHAIN_DEFAULT_CREATOR "BLUE000" CACHE STRING "Original creator copyright owner name")
```

---

## 2. CMakeスクリプト詳細設計 (`trustchain.cmake`)

ビルド構成時に、Git status のチェック、GitHub API からのコミットハッシュの取得、および Web API 経由でのトークン発行要求を処理するスクリプトです。

### 2.1. アルゴリズムと変数設計

1. **初期値設定**
   * `BUILD_IS_CUSTOMIZED` を `"0"`（公式）に初期化。
2. **Git status (ローカルの改ざん判定)**
   * `git status --porcelain` を実行し、未コミットの変更がある場合は `BUILD_IS_CUSTOMIZED = 1` に設定。
3. **GitHub API を用いた最新マスタのハッシュ取得**
   * インターネット接続を行い、以下の GitHub API エンドポイントから最新のコミットハッシュを取得する。
     ```
     https://api.github.com/repos/${TRUSTCHAIN_GITHUB_USER}/${TRUSTCHAIN_GITHUB_REPO}/commits/${TRUSTCHAIN_TARGET_BRANCH}
     ```
   * Windows環境の `curl` を用いて、レスポンスの JSON から `sha` フィールド（コミットハッシュ）を正規表現で抽出する。
   * **通信失敗・タイムアウト・権限不足（404/403）時の挙動**: `BUILD_IS_CUSTOMIZED = 1` に設定する。
4. **コミットハッシュの比較**
   * ローカルの現在ヘッドのコミットハッシュ（`git rev-parse HEAD`）と、GitHub API から取得した最新コミットハッシュを比較する。
   * **不一致（古いバージョン、あるいは独自コミット）時の挙動**: `BUILD_IS_CUSTOMIZED = 1` に設定する。
5. **TransCipher APIへのトークン発行要求**
   * `curl` コマンドを用いて、`TRUSTCHAIN_TOKEN_ISSUER_URL` に対して以下の JSON ペイロードで POST 送信する。
     ```json
     {
       "action": "generate",
       "system_name": "${PROJECT_NAME}",
       "build_secret": "${TRUSTCHAIN_BUILD_SECRET}",
       "build_hash": "${LOCAL_GIT_COMMIT_HASH}",
       "is_official": true (または false)
     }
     ```
   * 取得した JSON から API トークンをパースし、変数 `TRUSTCHAIN_API_TOKEN` に格納。パース失敗時は `"UNAUTHORIZED_TOKEN"` とする。

### 2.2. コンパイル定義（マクロ埋め込み）への設定
CMakeのビルドターゲットに対して以下のマクロを注入する。
```cmake
target_compile_definitions(${PROJECT_NAME} PRIVATE
    TRUSTCHAIN_BUILD_IS_CUSTOMIZED=${BUILD_IS_CUSTOMIZED}
    TRUSTCHAIN_API_TOKEN="${TRUSTCHAIN_API_TOKEN}"
    TRUSTCHAIN_CREATOR_NAME="${TRUSTCHAIN_DEFAULT_CREATOR}"
)
```

---

## 3. C++ コアクラス詳細設計 (`TrustChain::Core`)

GUIに依存せず、`QtCore` および `QtNetwork` のみを用いて起動時のオンライン認証を行うクラスです。

### 3.1. クラス定義・型定義 (`TrustChainCore.hpp`)

```cpp
#pragma once

#include <QString>
#include <QObject>
#include <QNetworkAccessManager>

namespace TrustChain {

// 認証およびビルド状態の判定結果
enum class AuthStatus {
    Normal,         // 公式ビルドかつ認証成功（通常起動）
    Watermarked,    // 非公式ビルド、オフライン、または通信エラー（ウォーターマーク表示で起動）
    Terminated      // サーバー側でトークンが無効化された（起動不可・強制終了）
};

class Core : public QObject {
    Q_OBJECT

public:
    explicit Core(QObject* parent = nullptr);

    // 起動時のオンライン認証を同期（ブロッキング）で実行する
    // タイムアウト: 3.0秒
    AuthStatus verifyToken();

    // 強制終了時のエラーダイアログの表示とプロセスの終了処理を行う
    static void terminateApplication(const QString& errorMessage = QString());

private:
    QString m_apiUrl;
    QString m_apiToken;
    bool m_buildIsCustomized;
};

} // namespace TrustChain
```

### 3.2. トークン検証処理の内部実装ロジック (`verifyToken`)
1. マクロで注入された `TRUSTCHAIN_BUILD_IS_CUSTOMIZED` が `1` の場合、即座に `AuthStatus::Watermarked` を返す。
2. ネットワーク通信部：
   * `QNetworkAccessManager` を使用し、`TRUSTCHAIN_TOKEN_ISSUER_URL` (トークン検証用エンドポイント) に対して POST リクエストを送信する。
   * JSON ペイロード:
     ```json
     {
       "action": "verify",
       "token": "埋め込まれたトークン"
     }
     ```
   * `QEventLoop` を用いて同期的にレスポンスを待機する（メインスレッドをブロックし、UIが立ち上がる前に検証を完了させるため）。
   * `QTimer` を併用し、**3.0秒**で応答がない場合はタイムアウトとし、自動的に `AuthStatus::Watermarked` （オフライン扱い）にフォールバックする。
3. 検証結果の解析：
   * サーバーから `{"status": "success", "valid": true}` が返された場合 ＝ `AuthStatus::Normal`。
   * サーバーから `{"status": "error"}` または有効性 `false` が返された場合（＝管理画面でトークンが無効化された場合） ＝ `AuthStatus::Terminated`。
   * その他のHTTPエラー、DNSエラー ＝ `AuthStatus::Watermarked`。

---

## 4. Qt6 拡張クラス詳細設計 (`TrustChain::QtHelper`)

Qt6 の GUI (QtWidgets) アプリケーションにウォーターマークを適用するためのヘルパークラスです。

### 4.1. インターフェース設計 (`TrustChainQt.hpp`)

```cpp
#pragma once

#include <QMainWindow>
#include "TrustChainCore.hpp"

namespace TrustChain {

class QtHelper {
public:
    // メインウィンドウに対して、認証結果に基づいたウォーターマーク制御を適用する
    static void applyWatermark(QMainWindow* window, AuthStatus status);
};

} // namespace TrustChain
```

### 4.2. UI適用ロジックの詳細
`applyWatermark` が呼び出された際、引数の `status` に基づいて以下の処理を行う。

```cpp
void QtHelper::applyWatermark(QMainWindow* window, AuthStatus status) {
    if (!window) return;

    // 1. 強制終了ルートの場合 (通常はCore側でハンドリングされるが安全策として)
    if (status == AuthStatus::Terminated) {
        Core::terminateApplication("このビルドはセキュリティ上の理由により無効化されています。");
        return;
    }

    // 2. ウォーターマーク表示ルートの場合 (改ざんビルド or オフライン or 通信エラー)
    if (status == AuthStatus::Watermarked || TRUSTCHAIN_BUILD_IS_CUSTOMIZED) {
        // ① タイトルバーの強制変更
        QString currentTitle = window->windowTitle();
        if (!currentTitle.contains("(Custom Build)")) {
            window->setWindowTitle(currentTitle + " (Custom Build)");
        }

        // ② ステータスバーへのコピーライトの強制常時表示
        QStatusBar* statusBar = window->statusBar();
        if (statusBar) {
            statusBar->showMessage(QString("© %1 (Original Creator)").arg(TRUSTCHAIN_CREATOR_NAME));
            
            // ユーザーが他のメッセージで上書きできないように強制するCSSスタイル
            statusBar->setStyleSheet(
                "color: #888888; "
                "background-color: #121214; "
                "font-weight: bold; "
                "border-top: 1px solid #2d2d30;"
            );
            
            // ステータスバーが非表示にされるのを防ぐ
            statusBar->setVisible(true);
        }
    }
}
```

---

## 5. アプリケーション終了処理 (`terminateApplication`)

トークンが無効化されている場合に実行される、安全かつ確実な強制終了のロジックです。

```cpp
void Core::terminateApplication(const QString& errorMessage) {
    // 1. 警告ダイアログの表示 (Qt環境が初期化されている場合)
    // QApplication::instance() が存在すれば QMessageBox を利用して表示
    QString displayMsg = errorMessage.isEmpty() 
        ? "セキュリティエラー: トークンが無効化されているか、改ざんが検出されたため、アプリケーションを起動できません。"
        : errorMessage;
        
    // 2. プロセスの強制シャットダウン
    // イベントループを完全に終了させ、終了コード -1 で exit する
    qFatal("%s", displayMsg.toUtf8().constData());
    std::exit(-1);
}
```
*(※ `qFatal` を呼ぶことで、Windows環境ではエラーログへの出力とともに安全にクラッシュまたは終了させることができます)*

---

## 6. バイナリ透かし詳細設計 (BinMarkManager 連携)

本フレームワークにおける第3の防壁「バイナリ透かし（Overlay Watermarking）」は、ソースコード上に依存を持たせず、ビルド完了後の実行ファイルに対して適用されます。
この処理と検証のロジックは、完全に独立した別ツールのモジュール [BinMarkManager](https://github.com/BLUE000/BinMarkManager.git) に分離されています。

### 6.1. データフォーマットと格納方式
* **テキストタグ方式**:
  抽出の安定性と柔軟性を高めるため、古いバイナリ構造（固定マジックナンバーと4バイト整数によるサイズ管理）は廃止し、バイナリ末尾に `[BM_START]` および `[BM_END]` のテキストタグを用いて可変長データを格納するサンドボックス形式を採用しています。
* **暗号化ペイロード (TransCipher 3.0.0 / TCF-TLV)**:
  格納される証拠データは、TransCipher Ver.3.0.0 によって暗号化・難読化されます。このバージョンから採用された TCF (TransCipher Format) の 64バイト固定長ヘッダと、TLV (Type-Length-Value) 構造の可変長ペイロードは、Base64文字列に変換されて格納されます。これによりメタデータ（Salt等）を安全にパッキングしています。
* **平文コピーライト**:
  暗号化されたデータと合わせて、バイナリエディタで視認可能な平文のコピーライトもテキストベースで直感的に格納されます。
