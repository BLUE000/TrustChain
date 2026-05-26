# =========================================================================
# TrustChain 認証・シークレット設定テンプレート
#
# 【使用方法】
# 1. 本ファイルを "trustchain_credentials.cmake" という名前でコピーします。
# 2. コピーしたファイルをプロジェクトの要件に合わせて編集します。
# 3. "trustchain_credentials.cmake" は機密情報が含まれるため、
#    絶対に Git にコミットしないでください（.gitignore に登録済み）。
# =========================================================================

# 1. 接続先 GitHub リポジトリ情報（出自証明に使用）
set(TRUSTCHAIN_GITHUB_USER "BLUE000" CACHE STRING "GitHub username or organization")
set(TRUSTCHAIN_GITHUB_REPO "TwitchFollowerChecker" CACHE STRING "GitHub repository name")
set(TRUSTCHAIN_TARGET_BRANCH "master" CACHE STRING "Target branch to check provenance (master/main)")

# 2. TransCipher Web API 接続情報
set(TRUSTCHAIN_TOKEN_ISSUER_URL "https://streamers-tool.sakura.ne.jp/TransCipher/index.php" CACHE STRING "URL for automated token issuance")
set(TRUSTCHAIN_BUILD_SECRET "BLUE000_BUILD_SECRET_KEY" CACHE STRING "Secret key for build system to request tokens")

# 3. コピーライト（ウォーターマーク）表示時の著作権所有者名
set(TRUSTCHAIN_DEFAULT_CREATOR "BLUE000" CACHE STRING "Original creator copyright owner name")
