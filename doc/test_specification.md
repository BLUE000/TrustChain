# TrustChain テスト仕様書 (Test Specification)

本仕様書は、C++セキュリティモジュール「**TrustChain**」の検証を目的とした、Google Test (GTest) および Qt Test (QTest) を用いたテストケースを定義します。

本仕様書は開発のV字モデルに準拠し、以下の関係性でテストを設計しています。
* **詳細設計** ───► **単体テスト** (GTest / QTest を用いた個別関数・クラスの検証)
* **基本設計** ───► **結合テスト** (GTest / QTest を用いたモジュール間・UI連携の検証)
* **要求仕様** ───► **総合テスト** (CMakeビルド時から実行時、ダミーサーバー連携までのシステムE2E検証)

---

## 1. テスト環境・検証戦略

### 1.1 使用フレームワークとアプローチ
* **Google Test (GTest)**: C++コアロジックの検証、およびデス・テスト（Death Test）機能による「トークン無効時の強制終了（プロセス終了）」の動作検証に使用します。
* **Qt Test (QTest)**: 起動時のオンライン認証（非同期/同期通信）のイベントループ処理、および `QMainWindow` へのタイトル変更・ステータスバーへのコピーライト（ウォーターマーク）出力状態の検証に使用します。
* **GUI表示の検証範囲**: 実際の画面描画を行う代わりに、QTestを用いて生成した `QMainWindow` オブジェクトに対してタイトル文字列や `QStatusBar` に設定されたメッセージ・CSSスタイルシートの値が「データとして正しく出力されているか」をアサートします（ヘッドレス環境でのCI/CD実行が可能です）。

### 1.2 テスト用ダミーサーバー (Mock API) の仕様
ネットワーク接続の有無やAPIの応答に左右されずテストを安定（決定論的）にするため、テストドライバは `MockNetworkAccessManager` を使用し、`TrustChain::Core` に対して以下のダミー応答（JSON）を返却して検証します。

1. **認証成功（Normal）応答**:
   ```json
   { "status": "success", "valid": true }
   ```
2. **トークン無効（Terminated）応答**:
   ```json
   { "status": "success", "valid": false }
   ```
3. **サーバーエラー / 不正応答**:
   HTTPステータス `500` や空のJSONなどを返却し、タイムアウトや通信切断と同等のフォールバック動作を誘発させます。

---

## 2. 単体テスト仕様 (Unit Test) ── 詳細設計に対応

詳細設計書で定義された `TrustChain::Core` および `TrustChain::QtHelper` の内部ロジック、およびメソッド単位の正確性を検証します。

| テストケースID | テスト対象機能 | テスト内容・手順 | 期待される結果 (アサート) |
| :--- | :--- | :--- | :--- |
| `UT_TC_CORE_001` | verifyToken() (認証成功) | `MockNetworkAccessManager` に「認証成功応答」を事前設定し、`verifyToken()` を呼び出す。 | 戻り値が `AuthStatus::Normal` であること。 |
| `UT_TC_CORE_002` | verifyToken() (トークン無効) | `MockNetworkAccessManager` に「トークン無効応答」を事前設定し、`verifyToken()` を呼び出す。 | 戻り値が `AuthStatus::Terminated` であること。 |
| `UT_TC_CORE_003` | verifyToken() (タイムアウト) | `MockNetworkAccessManager` のレスポンス遅延を **3.5秒** に設定し、`verifyToken()` を呼び出す。 | 内部タイマーによって3.0秒で処理が打ち切られ、`AuthStatus::Watermarked` が返ること。 |
| `UT_TC_CORE_004` | verifyToken() (サーバーエラー) | `MockNetworkAccessManager` のHTTPステータスを `500` エラーに設定し、`verifyToken()` を呼び出す。 | 通信失敗と判定され、`AuthStatus::Watermarked` が返ること。 |
| `UT_TC_CORE_005` | terminateApplication() | GTest の `EXPECT_DEATH`（Death Test）を用いて `terminateApplication()` を呼び出す。 | プロセスが正常に強制終了し、終了コードが `-1` であること。 |
| `UT_TC_QT_001` | applyWatermark() (Normal) | 空の `QMainWindow` インスタンスを作成し、`applyWatermark()` に `AuthStatus::Normal` を渡す。 | タイトルバーやステータスバーが変更されず、初期状態のままであること。 |
| `UT_TC_QT_002` | applyWatermark() (Watermarked) | `QMainWindow` インスタンスを作成し、`applyWatermark()` に `AuthStatus::Watermarked` を渡す。 | ① タイトルバーの末尾に `(Custom Build)` が付与されること。<br>② ステータスバーが表示状態（`isVisible`）になり、指定のコピーライト文言および強制用CSSスタイルが適用されていること。 |

---

## 3. 結合テスト仕様 (Integration Test) ── 基本設計に対応

基本設計で定義された「ビルドマクロによる事前フラグ」と「起動時オンライン認証」から「UI制御」に至る一連の機能連携とデータ遷移を検証します。

```text
  [ビルドマクロ設定]
         │ (TRUSTCHAIN_BUILD_IS_CUSTOMIZED)
         ▼
  [TrustChain::Core] ── (verifyToken実行 / ダミー通信) ──► AuthStatus 決定
         │
         ▼ (status を引き渡す)
  [TrustChain::QtHelper] ──────────────────────────────► QMainWindow UI自動加工
```

### 3.1 結合テストケース一覧

| テストケースID | テスト対象機能・連携 | テスト内容・手順 | 期待される結果 (アサート) |
| :--- | :--- | :--- | :--- |
| `IT_TC_FLOW_001` | 公式ビルド・正常起動フロー | 1. ビルド状態を `TRUSTCHAIN_BUILD_IS_CUSTOMIZED = 0` と仮定。<br>2. `MockNetworkAccessManager` から認証成功応答（valid: true）を返す。<br>3. `verifyToken()` の結果を `QtHelper::applyWatermark()` に適用する。 | アプリは強制終了せず起動し、`QMainWindow` はウォーターマークなし（公式表示）であること。 |
| `IT_TC_FLOW_002` | 改ざん・非公式ビルドフロー | 1. ビルド状態を `TRUSTCHAIN_BUILD_IS_CUSTOMIZED = 1`（改ざん判定）と設定。<br>2. `verifyToken()` を呼び出し、結果を `QtHelper::applyWatermark()` に適用する。 | ① 起動時のHTTP通信が行われないこと（通信がスキップされる）。<br>② `QMainWindow` にウォーターマーク（Custom Build表記、コピーライト）が強制表示されること。 |
| `IT_TC_FLOW_003` | オフライン/通信切断フロー | 1. ビルド状態は `0`（公式）とするが、ダミーサーバーとの接続をタイムアウト（通信不能）にする。<br>2. `verifyToken()` の結果を `QtHelper::applyWatermark()` に適用する。 | ① アプリケーションは終了せず起動すること。<br>② UIが「ウォーターマーク表示状態」に自動的に切り替わること。 |
| `IT_TC_FLOW_004` | トークン無効化フロー | 1. ビルド状態は `0` とし、ダミーサーバーから「トークン無効応答（valid: false）」を返す。<br>2. `verifyToken()` の判定結果に基づいて `applyWatermark()` を実行する（Death Test環境下）。 | アプリのUIが表示される前に、`Core::terminateApplication()` が呼び出され、終了コード `-1` で即座にプロセスが終了すること。 |

---

## 4. 総合テスト仕様 (System Test) ── 要求仕様（要件定義）に対応

実際の開発リポジトリと GitHub、および TransCipher API（ダミー）を連携させた、ビルド構成時からアプリ起動・終了までのエンドツーエンド (E2E) シナリオを検証します。

### ST_TC_SYS_001: クリーンな公式ビルドの出自証明および通常起動
* **テスト環境前提**:
  * ローカルのGitコミットハッシュが、GitHubリモートの `TRUSTCHAIN_TARGET_BRANCH`（`master`）の最新ハッシュと完全に一致している。
  * ローカルリポジトリに未コミットの変更がない。
  * ダミーサーバーが動作しており、有効なトークンを払い出し可能。
* **試験手順**:
  1. CMake の構成（Configure）を実行し、ビルドを生成する。
  2. 生成された実行ファイルを起動する。
* **期待される結果**:
  1. CMake実行ログに `BUILD_IS_CUSTOMIZED = 0` と表示され、トークン取得成功が表示されること。
  2. アプリケーションが起動し、タイトルバーやステータスバーは通常表示（ウォーターマークなし）のまま機能すること。

### ST_TC_SYS_002: ローカル改ざん（未コミット変更）時の出自非公式化と制限起動
* **テスト環境前提**:
  * 公式クローン状態から、ローカルのソースファイルを1行だけ意図的に改変（未コミットの変更がある状態）。
* **試験手順**:
  1. CMake を実行してビルドを生成する。
  2. 生成された実行ファイルを起動する。
* **期待される結果**:
  1. CMake実行ログに `BUILD_IS_CUSTOMIZED = 1` が出力され、制限されたAPIトークンがバイナリに埋め込まれること。
  2. アプリケーション起動時、タイトルバー末尾に `(Custom Build)` が表示され、ステータスバーに常時 `© BLUE000 (Original Creator)` が強制出力されること。

### ST_TC_SYS_003: リモートマスタ不一致（古いコミットまたは独自コミット）時の出自非公式化
* **テスト環境前提**:
  * ローカルはクリーン（未コミット変更なし）だが、GitHubリモート上の最新マスタコミットハッシュより古い（またはマスタの歴史上にない独自ローカルコミットである）状態。
* **試験手順**:
  1. CMake を実行してビルドを生成する。
  2. 生成された実行ファイルを起動する。
* **期待される結果**:
  1. CMakeがリモートハッシュの不一致を検出し、ログに `BUILD_IS_CUSTOMIZED = 1` を出力すること。
  2. アプリケーション起動時、起動オンライン認証で検証が通らず（または非公式トークン判定）、UIにウォーターマークが強制出力されること。

### ST_TC_SYS_004: 完全オフライン環境でのビルドと実行
* **テスト環境前提**:
  * ビルドPCおよび実行環境のネットワークアダプターを無効化（オフライン状態）。
* **試験手順**:
  1. CMake を実行してビルドを生成する。
  2. 生成された実行ファイルを起動する。
* **期待される結果**:
  1. CMakeがGitHub APIとの通信に失敗し、安全のために `BUILD_IS_CUSTOMIZED = 1` としてビルドを完了させること。
  2. アプリケーション起動時、オンライン認証のタイムアウト（3秒）が発生し、ウォーターマーク表示ルートで起動すること。

### ST_TC_SYS_005: サーバー側トークン無効化（ブラックリスト化）による即時利用停止
* **テスト環境前提**:
  * 公式クリーンビルドで作成された、一度は `Normal` 起動していたアプリを使用。
  * ダミーサーバー側の管理DBで、そのビルドに紐づくトークンを「無効（valid = false）」に変更する。
* **試験手順**:
  1. アプリケーションを起動する。
* **期待される結果**:
  1. 起動時オンライン認証のレスポンスで `valid = false` を受信すること。
  2. アプリのGUIが表示される前に、致命的エラーダイアログ（「このビルドはセキュリティ上の理由により無効化されています」）が表示され、プロセスが即座に強制終了すること。
