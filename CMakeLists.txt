cmake_minimum_required(VERSION 3.16)
project(TrustChain VERSION 2.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 1. 依存ライブラリの検索
find_package(Qt6 COMPONENTS Core Widgets Network REQUIRED)

# 2. 出自証明・トークン取得マクロモジュールのロード
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/trustchain.cmake)

# 3. TrustChain ライブラリターゲットの定義
set(TRUSTCHAIN_SOURCES
    src/TrustChainCore.cpp
    src/TrustChainQt.cpp
)

add_library(trustchain INTERFACE)

target_sources(trustchain INTERFACE
    ${TRUSTCHAIN_SOURCES}
)

# インクルードディレクトリのエクスポート
target_include_directories(trustchain INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 依存関係のリンク
target_link_libraries(trustchain INTERFACE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
)

# 4. 単独開発時またはテスト有効時のサブディレクトリロード
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    message(STATUS "[TrustChain] Building in standalone mode. Enabling tests.")
    
    # ローカル検証用の credentials がなければテンプレートからコピーしてロード
    set(LOCAL_CREDS "${CMAKE_CURRENT_SOURCE_DIR}/trustchain_credentials.cmake")
    if (NOT EXISTS "${LOCAL_CREDS}")
        message(STATUS "[TrustChain] Generating local trustchain_credentials.cmake from template...")
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/trustchain_credentials.example.cmake"
            "${LOCAL_CREDS}"
            COPYONLY
        )
    endif()

    # 出自証明とマクロ注入のセットアップを trustchain ターゲットに適用
    trustchain_setup_target(trustchain)
    
    # テストディレクトリの追加
    add_subdirectory(test)

    # 動作確認用ダミーアプリケーションの追加
    add_subdirectory(dummy_app)
endif()
