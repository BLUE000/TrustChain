# TrustChain (トラストチェーン) ⛓️

## 1. TrustChain ってなに？
あなたが一生懸命作ったC++（Qt6）のアプリが、**「誰かに勝手に改造（改ざん）されて配られないようにする」**ための、かんたんで強力なガードマンのような道具です。

### 🌟 なにをしてくれるの？
1. **本物チェック（出自証明）**
   ビルドする（プログラムを完成させる）ときに、あなたのパソコンのプログラムが「GitHub（ネット上の本棚）にある本物のプログラムと完全に同じか」を自動でチェックします。
2. **秘密の合言葉（トークン）をもらう**
   本物だと確認できたら、外部の安全な難読化/認証API [TransCipher-Dist](https://github.com/BLUE000/TransCipher-Dist) から「本物の証明書（トークン）」を自動でもらって、アプリのなかにこっそり埋め込みます。
3. **動くときにダブルチェック**
   アプリが起動した瞬間に、埋め込まれた証明書が今も有効かをネットで確認します。
   * **本物のとき**: なにごともなく、普通にアプリが起動します。
   * **オフライン・改造版のとき**: 画面のタイトルに `(Custom Build)` と自動で書かれ、画面の下（ステータスバー）に消せないコピーライトが強制表示されます。
   * **悪用されて無効化されたとき**: エラーメッセージを出して、アプリを安全に**自動で強制終了**します。

---

## 2. つかうために必要なもの
* **Windows パソコン**
* **Qt6**（画面を作るための枠組み）と **C++20**（プログラミング言語）
* **CMake**（プログラムを組み立てるツール）
* **Git**（プログラムの歴史を記録するツール。コマンドが動く状態にしておいてください）

---

## 3. 超かんたん！つかいかた (3ステップ)

ポインタ（`*` や `&`）の意味がよくわからなくても大丈夫です！
以下の手順通りにファイルをコピーして、コードをそのまま貼り付ける（コピペする）だけで完璧に動きます。

### 📁 ステップ 1: フォルダに入れる
あなたの作りたいアプリのプロジェクトフォルダ（`CMakeLists.txt` がある場所）に、この `TrustChain` フォルダをそのまま丸ごとコピーして入れてください。

---

### 🔑 ステップ 2: 「ひみつの設定ファイル」を作る
1. `TrustChain` フォルダのなかにある `trustchain_credentials.example.cmake` というファイルをコピーします。
2. コピーしたファイルを、あなたのアプリのフォルダ（`CMakeLists.txt` と同じ場所）に貼り付け、名前を **`trustchain_credentials.cmake`** に変更します。
3. そのファイルをテキストエディタで開き、あなたの情報に変更します。
   ```cmake
   # あなたのGitHubのユーザー名と、アプリのリポジトリ（本棚）の名前を書きます
   set(TRUSTCHAIN_GITHUB_USER "あなたのユーザー名")
   set(TRUSTCHAIN_GITHUB_REPO "あなたのアプリ名")
   
   # TransCipherサーバーのURLと、ビルド用のひみつの合言葉を書きます
   set(TRUSTCHAIN_TOKEN_ISSUER_URL "https://streamers-tool.sakura.ne.jp/TransCipher/index.php")
   set(TRUSTCHAIN_BUILD_SECRET "あなたのひみつの鍵")
   ```
> 💡 **TransCipher とは？**
> アプリケーションの認証情報を安全に難読化・復元するための公開ライブラリおよび仕組みです。詳細については、公開用ディストリビューションリポジトリ [TransCipher-Dist](https://github.com/BLUE000/TransCipher-Dist) をご覧ください。
> 
> ⚠️ **超重要！**
> この `trustchain_credentials.cmake` には、あなただけのひみつの鍵が書かれています。絶対にインターネット（GitHubなど）に公開（コミット）しないでください！
> （自動で Git の管理から外す設定がされています）

---

### 📝 ステップ 3: コードをコピペする

#### ① あなたのアプリの `CMakeLists.txt` の書き換え
あなたのアプリの `CMakeLists.txt` の一番下に、以下の**魔法の3行**を貼り付けてください。

```cmake
# TrustChainフォルダを読み込んで、ビルド時チェックを自動でオンにします！
add_subdirectory(TrustChain)
trustchain_setup_target(あなたのアプリのターゲット名)
target_link_libraries(あなたのアプリのターゲット名 PRIVATE trustchain)
```

#### ② あなたのアプリの `main.cpp` の書き換え
アプリが起動したときに、最初にチェックを走らせます。`main.cpp` の `main` 関数の最初の方に以下をコピペします。

```cpp
#include "TrustChainCore.hpp"
#include "TrustChainQt.hpp"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 1. ネットで本物か確認するガードマン（Core）を作ります
    TrustChain::Core guard;
    
    // 2. サーバーに「このアプリ本物？」と問い合わせます
    TrustChain::AuthStatus status = guard.verifyToken();

    // 3. メイン画面（MainWindow）を作ります
    MainWindow w;

    // 4. ガードマンの判定をもとに、画面にウォーターマークをつけます（ポインタをそのまま渡せば動きます）
    TrustChain::QtHelper::applyWatermark(&w, status);

    w.show();
    return a.exec();
}
```

これで、あなたのアプリの出自証明とセキュリティの組み込みは完了です！🎉

---

## 4. 🛡️ トークン `\0` (ヌル文字) 汚染に対する安心の防衛
プログラミングでよくあるバグに、文字データのなかに `\0`（ヌル文字という、文字の終わりを表す特殊なマーク）が紛れ込んで、データが途中で切れたり壊れたりする問題があります。

TrustChain は、もらった合言葉（トークン）の中にこの `\0` や、変な見えない文字が混ざっていないかを**自動で超厳格にスキャンして排除**します。もし変なデータを見つけたら、悪意のある攻撃とみなして安全に動作を制限するため、安心して利用できます。

---

## 5. 権利表記についてのお願い
本ツール（TrustChain）自体は、画面上に自身の権利表記（ウォーターマーク）を強制表示することはありません。あくまで「TrustChainを組み込んだあなたのアプリの権利」を守るための黒子として働きます。

そのため、**TrustChainを使用してアプリを配布する際は、あなたのアプリの「Readme」や「ライセンス表記画面」などに、以下のサードパーティ・ライセンス表記をそのままコピペして記載していただくようお願いいたします。**

```text
This software uses TrustChain Module. Copyright (c) 2026 BLUE000.
Includes TransCipher, Copyright (c) 2026 BLUE000. (https://github.com/BLUE000/TransCipher-Dist)
Includes BinMarkManager, Copyright (c) 2026 BLUE000. (https://github.com/BLUE000/BinMarkManager)

Qt is licensed under the GNU Lesser General Public License (LGPL) version 3.
Copyright (C) 2024 The Qt Company Ltd and other contributors.
(https://www.qt.io/licensing/)
```

（※上記を記載していただくだけで問題ありません。プログラム側で画面上に強制表示させることはありませんのでご安心ください）

---

## 6. ライセンス (お約束ごと)
本ソフトウェアは **MITライセンス** のもとで公開されています。

### 📦 サードパーティ製ライブラリのライセンス
本ソフトウェアは、一部の機能において **Qt6** ライブラリを使用しています。
* **Qt6**: [GNU Lesser General Public License (LGPL) version 3](https://www.gnu.org/licenses/lgpl-3.0.html) または [GNU General Public License (GPL) version 3](https://www.gnu.org/licenses/gpl-3.0.html) などのライセンスのもとで提供されています。詳細は [Qt Licensing](https://www.qt.io/licensing/) をご確認ください。

### ⚠️ 無保証（むほしょう）について
本ソフトウェアは「現状有姿（今のままの形）」で提供され、特定の目的への適合性を含め、**一切の保証をいたしません。** 作者（BLUE000）は、本ソフトウェアを使用したこと、または使用できなかったことによって発生したいかなるトラブルや損害についても、**一切の責任を負いません。** 自己責任の上で安全にご利用ください。
詳細は同梱の `LICENSE` ファイルをご覧ください。
