#include <gtest/gtest.h>
#include "TrustChainCore.hpp"
#include "TrustChainQt.hpp"
#include <QStatusBar>

class TrustChainQtTest : public ::testing::Test {
protected:
    void SetUp() override {
        window = new QMainWindow();
        window->setWindowTitle("Twitch Follower Checker");
    }

    void TearDown() override {
        delete window;
    }

    QMainWindow* window;
};

// UT_TC_QT_001: 通常ステータス (Normal) 時は UI に変化がないこと
TEST_F(TrustChainQtTest, ApplyWatermark_NormalStatus_NoChanges) {
    TrustChain::QtHelper::applyWatermark(window, TrustChain::AuthStatus::Normal);

    // タイトルは変更されない
    EXPECT_EQ(window->windowTitle(), "Twitch Follower Checker");

    // ステータスバーは変更されない (初期化前ならnullptrか空メッセージ)
    QStatusBar* bar = window->statusBar();
    EXPECT_TRUE(bar->currentMessage().isEmpty());
}

// UT_TC_QT_002: 警告ステータス (Watermarked) 時はタイトルバーとステータスバーにウォーターマークが強制適用されること
TEST_F(TrustChainQtTest, ApplyWatermark_WatermarkedStatus_AppliesWatermark) {
    // 警告ステータスを適用
    TrustChain::QtHelper::applyWatermark(window, TrustChain::AuthStatus::Watermarked);

    // タイトル末尾にタグが付与されていること
    EXPECT_TRUE(window->windowTitle().endsWith("(Custom Build)"));
    EXPECT_TRUE(window->windowTitle().contains("Twitch Follower Checker"));

    // ステータスバーが強制表示され、コピーライト文言が入っていること
    QStatusBar* bar = window->statusBar();
    ASSERT_NE(bar, nullptr);
    EXPECT_FALSE(bar->isHidden()); // ヘッドレス環境でも安全にアサート可能
    
    // コピーライト文字列が含まれていること
    QString expectedMessage = "© BLUE000 (Original Creator)";
    EXPECT_TRUE(bar->currentMessage().startsWith(expectedMessage));

    // スタイルシートが適用されていること
    EXPECT_FALSE(bar->styleSheet().isEmpty());
    EXPECT_TRUE(bar->styleSheet().contains("color: #888888;"));
    EXPECT_TRUE(bar->styleSheet().contains("background-color: #121214;"));
}

// UT_TC_QT_003: 強制終了ステータス (Terminated) 時の Death Test 連携
TEST(TrustChainQtDeathTest, ApplyWatermark_TerminatedStatus_Exits) {
    QMainWindow dummyWindow;
    
    // Terminated を渡した際に、プロセスが安全にアボートされることをアサート (Death Test)
    EXPECT_DEATH(
        TrustChain::QtHelper::applyWatermark(&dummyWindow, TrustChain::AuthStatus::Terminated),
        ".*無効化されているため.*"
    );
}
