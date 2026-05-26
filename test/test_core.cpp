#include <gtest/gtest.h>
#include "TrustChainCore.hpp"
#include "MockNetworkAccessManager.hpp"
#include <QEventLoop>
#include <QTimer>

class TrustChainCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // テスト用のモックネットワークマネージャーを用意
        mockManager = new MockNetworkAccessManager();
        core = new TrustChain::Core();
        core->setNetworkAccessManager(mockManager);
        core->setBuildIsCustomized(false); // オンライン検証テスト用に明示的に公式ビルド状態に設定

        // テスト用API URLの期待値を設定
        testUrl = QUrl(TRUSTCHAIN_TOKEN_ISSUER_URL);
    }

    void TearDown() override {
        delete core;
        delete mockManager;
    }

    TrustChain::Core* core;
    MockNetworkAccessManager* mockManager;
    QUrl testUrl;
};

// UT_TC_CORE_001: 正常系 (有効なトークンでの認証成功)
TEST_F(TrustChainCoreTest, VerifyToken_Success) {
    QByteArray successJson = R"({"status": "success", "valid": true})";
    mockManager->setExpectedResponse(testUrl, 200, successJson);

    // テスト対象の実行 (setBuildIsCustomized(false) により必ずオンライン検証が走る)
    TrustChain::AuthStatus status = core->verifyToken();
    
    EXPECT_EQ(status, TrustChain::AuthStatus::Normal);
}

// UT_TC_CORE_002: トークン無効判定時の強制終了ステータス
TEST_F(TrustChainCoreTest, VerifyToken_InvalidToken_Revoked) {
    QByteArray revokedJson = R"({"status": "success", "valid": false})";
    mockManager->setExpectedResponse(testUrl, 200, revokedJson);

    TrustChain::AuthStatus status = core->verifyToken();
    EXPECT_EQ(status, TrustChain::AuthStatus::Terminated);
}

// UT_TC_CORE_003: オンライン検証タイムアウト (3秒で打ち切り)
TEST_F(TrustChainCoreTest, VerifyToken_Timeout) {
    QByteArray successJson = R"({"status": "success", "valid": true})";
    mockManager->setExpectedResponse(testUrl, 200, successJson);
    mockManager->setResponseDelay(3500); // 3.5秒遅延させる

    TrustChain::AuthStatus status = core->verifyToken();
    // タイムアウトした場合は Watermarked になる
    EXPECT_EQ(status, TrustChain::AuthStatus::Watermarked);
}

// UT_TC_CORE_004: サーバーエラー (HTTP 500) 時の安全フォールバック
TEST_F(TrustChainCoreTest, VerifyToken_ServerError) {
    QByteArray errorJson = R"({"status": "error", "message": "Internal Server Error"})";
    mockManager->setExpectedResponse(testUrl, 500, errorJson);

    TrustChain::AuthStatus status = core->verifyToken();
    // サーバーエラー時はウォーターマーク表示ルートへ倒す
    EXPECT_EQ(status, TrustChain::AuthStatus::Watermarked);
}

// UT_TC_CORE_005: ヌル文字 (\0) 汚染に対する防御機能の検証
TEST_F(TrustChainCoreTest, ValidateTokenSecurity_NullCharContamination) {
    // 1. ヌル文字が途中に含まれるトークンを検証
    QString contaminatedToken = QString("TOKEN_XYZ") + QChar('\0') + QString("Contamination");
    
    // Coreの非公開メソッドを検証するため、Coreのインスタンスを利用して
    // ヌル混入のバリデーションを間接的に検証する。
    // m_apiToken をこの汚染されたものに書き換えることはマクロ定義経由のため通常困難ですが、
    // テスト用の派生クラスまたは validateTokenSecurity をパブリックに擬似テストする。
    
    // 直接 validateTokenSecurity を検証するため、テスト用ヘルパーマクロ
    // (またはCore内部のヌル文字検知) が動作することを確認する。
    // 今回は安全対策として validateTokenSecurity を public にしたのと同様にテスト可能。
    
    // Coreオブジェクトを直接叩く
    bool isContaminatedSafe = core->validateTokenSecurity(contaminatedToken);
    EXPECT_FALSE(isContaminatedSafe);

    // 2. UTF-8にシリアライズした際にヌルバイトが混入する場合の検証
    QString utf8NullToken = "SAFE_TOKEN";
    utf8NullToken.append('\0');
    EXPECT_FALSE(core->validateTokenSecurity(utf8NullToken));

    // 3. 制御文字（改行やインビジブル制御文字）が含まれる場合の検証
    QString ctrlCharToken = "TOKEN_XYZ\x07_BAD"; // ベル文字 (0x07) の混入
    EXPECT_FALSE(core->validateTokenSecurity(ctrlCharToken));

    // 4. クリーンな通常のトークンは安全と判定されること
    QString cleanToken = "BLUE000_SECURE_TOKEN_12345_ABCDE";
    EXPECT_TRUE(core->validateTokenSecurity(cleanToken));
}

// UT_TC_CORE_006: terminateApplication のデス・テスト (Death Test)
TEST(TrustChainCoreDeathTest, TerminateApplication_ExitsProcess) {
    // GTestの Death Test を用いて、std::exit(-1) によるプロセスの強制終了をアサート
    // qFatal は終了コードを伴って強制終了を発生させるため、正常に終了することを検証
    EXPECT_DEATH(
        TrustChain::Core::terminateApplication("TEST_ERROR_MESSAGE"),
        ".*TEST_ERROR_MESSAGE.*"
    );
}

// UT_TC_CORE_007: 非公式ビルド設定時のオンライン検証スキップ検証
TEST_F(TrustChainCoreTest, VerifyToken_CustomizedBuild_SkipsOnline) {
    core->setBuildIsCustomized(true); // 非公式ビルド状態を模擬

    // サーバーには「成功応答」を設定しておくが、通信はスキップされるはず
    QByteArray successJson = R"({"status": "success", "valid": true})";
    mockManager->setExpectedResponse(testUrl, 200, successJson);

    TrustChain::AuthStatus status = core->verifyToken();
    // 通信をスキップして即座に Watermarked になること
    EXPECT_EQ(status, TrustChain::AuthStatus::Watermarked);
}
