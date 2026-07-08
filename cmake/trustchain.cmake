# =========================================================================
# TrustChain CMake 出自証明 & 自動トークン取得スクリプト
#
# このスクリプトは、ビルド時に GitHub リモートマスタとローカル環境を比較し、
# 出自証明（Provenance Verification）を行い、外部の TransCipher API から
# セキュアなトークンを取得して C++ バイナリにマクロとして安全に注入します。
# =========================================================================

function(trustchain_setup_target TARGET_NAME)
    # 1. 認証情報ファイルのロード（Git管理外）
    set(CREDENTIALS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/trustchain_credentials.cmake")
    if(NOT EXISTS "${CREDENTIALS_FILE}")
        set(CREDENTIALS_FILE "${CMAKE_SOURCE_DIR}/trustchain_credentials.cmake")
    endif()

    if(EXISTS "${CREDENTIALS_FILE}")
        include("${CREDENTIALS_FILE}")
        message(STATUS "[TrustChain] Loaded credentials from: ${CREDENTIALS_FILE}")
    else()
        message(WARNING "[TrustChain] credentials file NOT found! Using fallback configuration.")
        set(TRUSTCHAIN_GITHUB_USER "BLUE000")
        set(TRUSTCHAIN_GITHUB_REPO "TrustChain")
        set(TRUSTCHAIN_TARGET_BRANCH "master")
        set(TRUSTCHAIN_TOKEN_ISSUER_URL "https://streamers-tool.sakura.ne.jp/TransCipher/index.php")
        set(TRUSTCHAIN_BUILD_SECRET "MOCK_SECRET")
        set(TRUSTCHAIN_DEFAULT_CREATOR "BLUE000")
    endif()

    set(BUILD_IS_CUSTOMIZED "0")
    set(LOCAL_GIT_COMMIT_HASH "UNKNOWN_HASH")

    # 2. ローカル Git の状態を取得
    find_package(Git)
    if(GIT_FOUND)
        # 現在のローカルブランチ名を取得してターゲットブランチを動的に切り替え
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE CURRENT_GIT_BRANCH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT "${CURRENT_GIT_BRANCH}" STREQUAL "" AND NOT "${CURRENT_GIT_BRANCH}" STREQUAL "HEAD")
            set(TRUSTCHAIN_TARGET_BRANCH "${CURRENT_GIT_BRANCH}")
            message(STATUS "[TrustChain] Target branch dynamically set to active branch: ${TRUSTCHAIN_TARGET_BRANCH}")
        endif()

        # 未コミットのローカル変更があるかチェック
        execute_process(
            COMMAND ${GIT_EXECUTABLE} status --porcelain
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        # ローカル HEAD のコミットハッシュを取得
        execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            OUTPUT_VARIABLE LOCAL_GIT_COMMIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        if(NOT "${GIT_STATUS}" STREQUAL "")
            set(BUILD_IS_CUSTOMIZED "1")
            message(STATUS "[TrustChain] Local modifications detected (dirty status).")
        endif()
    else()
        set(BUILD_IS_CUSTOMIZED "1")
        message(WARNING "[TrustChain] Git executable not found. Set build to customized.")
    endif()

    # 3. Git ls-remoteからリモートリポジトリに現在のコミットが存在するか確認 (出自証明)
    message(STATUS "[TrustChain] Querying remote git for origin verification of commit: ${LOCAL_GIT_COMMIT_HASH}")
    
    execute_process(
        COMMAND ${GIT_EXECUTABLE} ls-remote origin
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_REMOTE_RESPONSE
        RESULT_VARIABLE GIT_REMOTE_RESULT
        TIMEOUT 10
    )

    if (GIT_REMOTE_RESULT EQUAL 0)
        # 4. 出自判定: リモートの出力にローカルHEADのコミットハッシュが含まれているか確認
        if (GIT_REMOTE_RESPONSE MATCHES "${LOCAL_GIT_COMMIT_HASH}")
            message(STATUS "[TrustChain] Origin verification succeeded: Commit exists on remote.")
        else()
            set(BUILD_IS_CUSTOMIZED "1")
            message(STATUS "[TrustChain] Origin verification failed: Commit does NOT exist on remote.")
        endif()
    else()
        # 通信エラー、権限不足、オフライン等の場合は安全側に倒して非公式判定
        set(BUILD_IS_CUSTOMIZED "1")
        message(WARNING "[TrustChain] Failed to contact remote git repository. Result: ${GIT_REMOTE_RESULT}")
    endif()

    # 5. TransCipher APIから自動トークン取得または事前生成トークンの利用
    set(TRANSCIPHER_API_TOKEN "UNAUTHORIZED_TOKEN")

    if (DEFINED TRUSTCHAIN_PREGENERATED_TOKEN AND NOT "${TRUSTCHAIN_PREGENERATED_TOKEN}" STREQUAL "")
        message(STATUS "[TrustChain] Using pre-generated API token.")
        set(TRANSCIPHER_API_TOKEN "${TRUSTCHAIN_PREGENERATED_TOKEN}")
    else()
        # POST用 JSON ペイロード構築
        set(IS_OFFICIAL_STR "true")
        if (BUILD_IS_CUSTOMIZED EQUAL 1)
            set(IS_OFFICIAL_STR "false")
        endif()
        
        set(CURL_JSON_PAYLOAD "{\"action\":\"generate\",\"system_name\":\"${PROJECT_NAME}\",\"build_secret\":\"${TRUSTCHAIN_BUILD_SECRET}\",\"build_hash\":\"${LOCAL_GIT_COMMIT_HASH}\",\"is_official\":${IS_OFFICIAL_STR}}")
        
        message(STATUS "[TrustChain] Requesting API Token from: ${TRUSTCHAIN_TOKEN_ISSUER_URL}")
        
        execute_process(
            COMMAND curl -s -X POST -H "Content-Type: application/json" -d "${CURL_JSON_PAYLOAD}" ${TRUSTCHAIN_TOKEN_ISSUER_URL}
            OUTPUT_VARIABLE FETCHED_JSON
            RESULT_VARIABLE CURL_TOKEN_RESULT
            TIMEOUT 10
        )
        
        # 応答JSONからトークン値を抽出
        if (CURL_TOKEN_RESULT EQUAL 0 AND FETCHED_JSON MATCHES "\"status\"[ \t\r\n]*:[ \t\r\n]*\"success\"" AND FETCHED_JSON MATCHES "\"token\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"")
            set(RAW_TOKEN "${CMAKE_MATCH_1}")
            # 余分な空白、改行等を厳密にトリミング (STRIP) し、マクロへの \0 混入を防ぐ
            string(STRIP "${RAW_TOKEN}" CLEAN_TOKEN)
            set(TRANSCIPHER_API_TOKEN "${CLEAN_TOKEN}")
            message(STATUS "[TrustChain] Token successfully retrieved and trimmed.")
        else()
            message(WARNING "[TrustChain] Failed to retrieve token from server. App may function with limitations. Response: ${FETCHED_JSON}")
        endif()
    endif()

    # 6. コンパイル定義（マクロ埋め込み）への設定
    # ダブルクォーテーションで安全に囲み、コンパイラへ通常の文字列定数として渡します。
    get_target_property(TARGET_TYPE ${TARGET_NAME} TYPE)
    if (TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
        set(SCOPE INTERFACE)
    else()
        set(SCOPE PRIVATE)
    endif()

    target_compile_definitions(${TARGET_NAME} ${SCOPE}
        TRUSTCHAIN_BUILD_IS_CUSTOMIZED=${BUILD_IS_CUSTOMIZED}
        TRUSTCHAIN_API_TOKEN="${TRANSCIPHER_API_TOKEN}"
        TRUSTCHAIN_CREATOR_NAME="${TRUSTCHAIN_DEFAULT_CREATOR}"
        TRUSTCHAIN_TOKEN_ISSUER_URL="${TRUSTCHAIN_TOKEN_ISSUER_URL}"
        TRUSTCHAIN_VERSION="${PROJECT_VERSION}"
    )
    
    message(STATUS "[TrustChain] Injected compile definitions successfully.")
    message(STATUS "  - TRUSTCHAIN_BUILD_IS_CUSTOMIZED = ${BUILD_IS_CUSTOMIZED}")
    message(STATUS "  - TRUSTCHAIN_API_TOKEN = (secured)")
    message(STATUS "  - TRUSTCHAIN_CREATOR_NAME = ${TRUSTCHAIN_DEFAULT_CREATOR}")
endfunction()
