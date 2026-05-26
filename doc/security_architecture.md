# Twitch セキュリティ & 改ざん検知システム アーキテクチャ解説書

本ドキュメントは、別プロジェクト（`TwitchChannelPoint`）で実装されている、難読化API（TransCipher）を活用した「改ざん検知（Tamper Detection）」および「自動認証トークン発行システム」の設計・仕様をまとめた技術資料です。

※本ファイルはGit管理外（`.gitignore`に登録済み）であり、公開リポジトリにはコミットされません。将来的に非公開リポジトリやプライベートなナレッジベースで管理することを想定しています。

---

## 1. システム全体設計

このセキュリティシステムは、**C++クライアント（ビルド時）**、**自前の外部Web API（PHP/さくらサーバー）**、**Discord通知（Webhook）**の3層が連携して動作します。

最大の強みは、**「機密情報（Discord Webhook URLなど）をC++クライアントコードに一切持たせず、サーバー側で安全に隠蔽・処理する」**という点にあります。

```
[開発・ビルド環境 (CMake)]
       │
       │ 1. Gitの差分チェック (未コミット変更や独自コミットの有無)
       ▼
 ├─ BUILD_IS_CUSTOMIZED = 0 (クリーン/公式)
 └─ BUILD_IS_CUSTOMIZED = 1 (改ざんあり/非公式)
       │
       │ 2. HTTP POST リクエスト送信 (APIトークン要求)
       │    - システム名: TwitchChannelPointManager
       │    - 秘密鍵: BLUE000_BUILD_SECRET_KEY
       │    - 公式ビルドフラグ (is_official: true/false)
       ▼
[外部認証用 Web API (さくらインターネット PHP)]
       │
       ├─ (is_official: false の場合)
       │  └─ 3. Discord Webhook 経由でアラート通知 🚨
       │         - 「非公式ビルド/トークン発行」を通知
       ▼
 4. 生成した「APIトークン」をC++ビルド環境へ返却
       │
       │ 5. トークンと BUILD_IS_CUSTOMIZED フラグをマクロとして埋め込む
       ▼
[コンパイル・製品実行ファイル]
       │
       └─ 起動時にマクロ判定
          └─ カスタムビルドの場合:
             - ウィンドウタイトルに「(Custom Build)」を強制付与
             - ステータスバーに「© BLUE000 (Original Creator)」を強制表示
```

---

## 2. 各層における詳細仕様と実装コード

### 2.1. ビルド時（CMake）の改ざんチェック処理

CMakeLists.txt 内で Git 状態を取得し、非公式ビルドであるかを自動判別します。

```cmake
# ==========================================
# ビルド時のウォーターマーク（改変検知）機能
# ==========================================
set(BUILD_IS_CUSTOMIZED "0")
find_package(Git)
if(GIT_FOUND)
    # 1. 未コミットのローカル変更があるかチェック
    execute_process(
        COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    # 2. 公式タグからのズレ（追加コミットやdirty状態）があるかチェック
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --always --dirty
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    if(NOT "${GIT_STATUS}" STREQUAL "")
        set(BUILD_IS_CUSTOMIZED "1")
    elseif("${GIT_DESCRIBE}" MATCHES "-dirty" OR "${GIT_DESCRIBE}" MATCHES "-g[0-9a-f]+")
        # -dirtyが含まれる、またはタグ以降の追加コミットがある場合
        set(BUILD_IS_CUSTOMIZED "1")
    endif()
endif()
```

### 2.2. Web API連携による自動トークン発行 ＆ Discord通知

CMakeがさくらサーバーのPHP APIに対してリクエストを送信し、API連携用のトークンを取得します。

```cmake
# ==========================================
# PHP Web APIからの自動トークン取得
# ==========================================
set(TRANSCIPHER_TOKEN_ISSUER_URL "https://streamers-tool.sakura.ne.jp/TransCipher/index.php" CACHE STRING "URL for automated token issuance")
set(TRANSCIPHER_BUILD_SECRET "BLUE000_BUILD_SECRET_KEY" CACHE STRING "Secret password for the build system to request tokens")

set(TRANSCIPHER_API_TOKEN "UNAUTHORIZED_TOKEN")

# JSON文字列の構築
set(CURL_JSON_PAYLOAD "{\"action\":\"generate\",\"system_name\":\"${TRANSCIPHER_SYSTEM_NAME}\",\"build_secret\":\"${TRANSCIPHER_BUILD_SECRET}\",\"build_hash\":\"${GIT_DESCRIBE}\",\"is_official\":")

if (BUILD_IS_CUSTOMIZED EQUAL 0)
    message(STATUS "Official Build: Requesting standard API token...")
    string(APPEND CURL_JSON_PAYLOAD "true}")
else()
    message(STATUS "Customized Build: Requesting restricted API token...")
    string(APPEND CURL_JSON_PAYLOAD "false}")
endif()

# PHPサーバーへPOSTリクエストを送信してトークン取得
execute_process(
    COMMAND curl -s -X POST 
            -H "Content-Type: application/json"
            -d "${CURL_JSON_PAYLOAD}"
            ${TRANSCIPHER_TOKEN_ISSUER_URL}
    OUTPUT_VARIABLE FETCHED_JSON
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE CURL_RESULT
)

# レスポンスJSONからトークン値を抽出
if (CURL_RESULT EQUAL 0 AND FETCHED_JSON MATCHES "\"status\"[ \t\r\n]*:[ \t\r\n]*\"success\"" AND FETCHED_JSON MATCHES "\"token\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"")
    set(TRANSCIPHER_API_TOKEN "${CMAKE_MATCH_1}")
    message(STATUS "Successfully retrieved TransCipher API Token: ${TRANSCIPHER_API_TOKEN}")
else()
    message(WARNING "Failed to retrieve API Token. App may not function correctly.")
endif()
```

#### 🛡️ サーバー（PHP）側の挙動（推測されるロジック）
* さくらインターネットサーバー上の `index.php` は、受け取った `is_official` が `false` である場合、または新規に非公式トークンを払い出す際に、**PHP内部に設定されたWebhook URL** を使って Discord に通知を送ります。
* **メリット**: Webhookの認証キーはさくらサーバーのファイル内にあるため、どれだけC++アプリがリバースエンジニアリングされても、Discord通知用のWebhook URLが他人に盗まれることはありません。

### 2.3. C++アプリ側でのウォーターマーク（画面）制御

ビルド時にCMakeから注入されたマクロ `BUILD_IS_CUSTOMIZED` に基づいて、GUIの挙動を直接変化させます。

```cmake
# CMakeLists.txt でのコンパイル定義注入
target_compile_definitions(${PROJECT_NAME} PRIVATE
    BUILD_IS_CUSTOMIZED=${BUILD_IS_CUSTOMIZED}
    TRANSCIPHER_API_TOKEN="${TRANSCIPHER_API_TOKEN}"
)
```

```cpp
// src/gui/MainWindow.cpp 内での制御
#ifndef BUILD_IS_CUSTOMIZED
#define BUILD_IS_CUSTOMIZED 0
#endif

MainWindow::MainWindow(Application* app, QWidget* parent)
    : QMainWindow(parent)
{
    if (BUILD_IS_CUSTOMIZED) {
        // ① タイトルバーの改変表示
        setWindowTitle(QString("Twitch Channel Point Manager - %1 (Custom Build)").arg(APP_VERSION_STRING));
        
        // ② ステータスバーへのコピーライト常時強制表示
        statusBar()->showMessage("© BLUE000 (Original Creator)");
        statusBar()->setStyleSheet("color: #888888; background-color: #121214;");
    } else {
        setWindowTitle(QString("Twitch Channel Point Manager - %1").arg(APP_VERSION_STRING));
    }
    
    // ...以降の初期化処理...
}
```

---

## 3. 本プロジェクト（TwitchFollowerChecker）への移植・再利用手順

新しく開発する `TwitchFollowerChecker` にこの改ざん検知・トークン発行を組み込む場合は、以下の手順のみで移植が完了します。

1. **`CMakeLists.txt` の編集**:
   - 上述の「改ざん検知機能」および「自動トークン取得」のCMakeスクリプトブロックをコピーして挿入します。
   - `SAFE_APP_NAME` などから生成するシステム名（`system_name`）を `"TwitchFollowerChecker"` に変更します。これにより、サーバー側で別アプリとして識別されます。
2. **`MainWindow.cpp` の編集**:
   - `BUILD_IS_CUSTOMIZED` の有無によるタイトルバーとステータスバーの制御ロジックをコンストラクタに追加します。
3. **さくらサーバーのPHP側の対応**:
   - `system_name` として新しく `"TwitchFollowerChecker"` から要求が来た際にも、トークン発行およびDiscord通知が正常にルーティングされるように、必要に応じてPHP側のホワイトリストや設定をアップデートしてください。
