cmake_minimum_required(VERSION 3.16)
project(TrustChainTest LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Qt6 パッケージのロード
find_package(Qt6 COMPONENTS Core Widgets Network Test REQUIRED)

# Google Test の取得 (FetchContent を使用して自動ダウンロード)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# ランタイムライブラリ競合の防止設定
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ソースファイルの定義
set(TEST_SOURCES
    test_main.cpp
    test_core.cpp
    test_qt.cpp
)

# 実行ターゲットの作成
add_executable(TrustChainTest ${TEST_SOURCES})

# コンパイル定義（マクロ）の仮定義
# テスト時は BUILD_IS_CUSTOMIZED=0（公式）と仮定して検証可能にするため 0 を設定
target_compile_definitions(TrustChainTest PRIVATE
    TRUSTCHAIN_BUILD_IS_CUSTOMIZED=0
    TRUSTCHAIN_API_TOKEN="MOCK_TEST_TOKEN_12345"
    TRUSTCHAIN_CREATOR_NAME="BLUE000"
    TRUSTCHAIN_TOKEN_ISSUER_URL="https://streamers-tool.sakura.ne.jp/TransCipher/index.php"
)

# インクルードディレクトリの指定
target_include_directories(TrustChainTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# リンク先ライブラリ
target_link_libraries(TrustChainTest PRIVATE
    trustchain
    gtest
    gtest_main
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Test
)

# テスト登録
enable_testing()
add_test(NAME TrustChainCoreAndQtTest COMMAND TrustChainTest)
