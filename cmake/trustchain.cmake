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

    # 3. GitHub APIからリモートマスタの最新コミットハッシュを取得 (出自証明)
    set(REMOTE_COMMIT_HASH "")
    set(GITHUB_API_URL "https://api.github.com/repos/${TRUSTCHAIN_GITHUB_USER}/${TRUSTCHAIN_GITHUB_REPO}/commits/${TRUSTCHAIN_TARGET_BRANCH}")
    
    message(STATUS "[TrustChain] Querying GitHub API for origin verification: ${GITHUB_API_URL}")
    
    # User-Agent ヘッダー（-A "TrustChain"）を付与しないと GitHub API は 403 Forbidden を返します
    execute_process(
        COMMAND curl -s -A "TrustChain-Build-Agent" -H "Accept: application/vnd.github.v3+json" ${GITHUB_API_URL}
        OUTPUT_VARIABLE GITHUB_RESPONSE
        RESULT_VARIABLE CURL_API_RESULT
        TIMEOUT 10
    )

    if (CURL_API_RESULT EQUAL 0 AND GITHUB_RESPONSE MATCHES "\"sha\"[ \t\r\n]*:[ \t\r\n]*\"([a-f0-9]+)\"")
        set(REMOTE_COMMIT_HASH "${CMAKE_MATCH_1}")
        message(STATUS "[TrustChain] GitHub Remote Hash: ${REMOTE_COMMIT_HASH}")
        message(STATUS "[TrustChain] Local Head Hash:   ${LOCAL_GIT_COMMIT_HASH}")
        
        # 4. 出自判定: ハッシュが不一致の場合は非公式（改ざん）扱い
        if (NOT "${LOCAL_GIT_COMMIT_HASH}" STREQUAL "${REMOTE_COMMIT_HASH}")
            set(BUILD_IS_CUSTOMIZED "1")
            message(STATUS "[TrustChain] Origin verification failed: Commit hash mismatch.")
        else()
            message(STATUS "[TrustChain] Origin verification succeeded.")
        endif()
    else()
        # 通信エラー、権限不足（404/403）、オフライン等の場合は安全側に倒して非公式判定
        set(BUILD_IS_CUSTOMIZED "1")
        message(WARNING "[TrustChain] Failed to contact GitHub API or invalid repository. Result: ${CURL_API_RESULT}")
    endif()

    # 5. TransCipher APIから自動トークン取得
    set(TRANSCIPHER_API_TOKEN "UNAUTHORIZED_TOKEN")
    
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

    # 6. コンパイル定義（マクロ埋め込み）への設定
    # ダブルクォーテーションで安全に囲み、コンパイラへ通常の文字列定数として渡します。
    target_compile_definitions(${TARGET_NAME} PRIVATE
        TRUSTCHAIN_BUILD_IS_CUSTOMIZED=${BUILD_IS_CUSTOMIZED}
        TRUSTCHAIN_API_TOKEN="${TRANSCIPHER_API_TOKEN}"
        TRUSTCHAIN_CREATOR_NAME="${TRUSTCHAIN_DEFAULT_CREATOR}"
        TRUSTCHAIN_TOKEN_ISSUER_URL="${TRUSTCHAIN_TOKEN_ISSUER_URL}"
    )
    
    message(STATUS "[TrustChain] Injected compile definitions successfully.")
    message(STATUS "  - TRUSTCHAIN_BUILD_IS_CUSTOMIZED = ${BUILD_IS_CUSTOMIZED}")
    message(STATUS "  - TRUSTCHAIN_API_TOKEN = (secured)")
    message(STATUS "  - TRUSTCHAIN_CREATOR_NAME = ${TRUSTCHAIN_DEFAULT_CREATOR}")
endfunction()
