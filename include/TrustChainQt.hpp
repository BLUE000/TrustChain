#pragma once

#include <QMainWindow>
#include "TrustChainCore.hpp"

namespace TrustChain {

/**
 * @brief Qt6 (QtWidgets) 向けのUIウォーターマーク自動制御クラス
 */
class QtHelper {
public:
    /**
     * @brief 認証結果に基づいて、対象のメインウィンドウにウォーターマーク（コピーライト強制表示・タイトル変更）を適用します
     * 
     * @param window 対象の QMainWindow
     * @param status Core::verifyToken() から取得した認証ステータス
     */
    static void applyWatermark(QMainWindow* window, AuthStatus status);
};

} // namespace TrustChain
