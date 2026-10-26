# SwayFlux3

優先度付きJobを並列実行するC++17ライブラリです。バージョンは0.2.0のプレビューで、APIは今後変更する可能性があります。
Schedulerが高優先度Jobの存在を通知し、各Jobが続行・一時停止・条件変更を選びます。
HTTP/HTTPSのストリーミングダウンロードと、ファイルからのPUT／POSTアップロードを利用できます。

## 構成

| 層 | CMakeターゲット | 内容 |
| --- | --- | --- |
| Core | `SwayFlux3::core` | Job・優先度・実行枠。C++標準ライブラリのみ |
| Communication | `SwayFlux3::communication` | 転送情報・進捗・受信先の契約 |
| HTTP | `SwayFlux3::http` | libcurl・zlib・Zstdを使うHTTP通信Job |
| 一括利用 | `SwayFlux3::swayflux3` | ビルドした上記の層をまとめて利用 |

公開APIは`include/swayflux3/`、実装は`src/`、利用例は[examples/](examples/)にあります。
独自の`Job`や`DownloadSink`を実装できます。ファイル形式の解析・描画機能は含みません。

## ビルド

C++17対応コンパイラーとCMake 3.24以上を用意してください。Coreだけの場合は通信ライブラリが不要です。

```sh
cmake -S . -B build-core -DSWAYFLUX3_BUILD_HTTP=OFF
cmake --build build-core --config Release
```

HTTPを使う場合はlibcurl 8.0以上、zlib、Zstdが必要です。
既定の受信自動展開には、gzip・Brotli・Zstdのデコーダーを備えたlibcurlを使用します。

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/dependencies
cmake --build build --config Release
```

Windows / Visual Studio 2022では、次のスクリプトで依存ライブラリと本体をビルドできます。
依存取得スクリプトはPython 3を利用し、CMakeがPATHになければプロジェクト内に取得します。

```powershell
./scripts/bootstrap-windows.ps1
./scripts/build-windows.ps1
# Coreのみ: ./scripts/build-windows.ps1 -CoreOnly
```

実機確認済みの環境はWindows x64 / MSVCです。Linux・macOS、HTTP/2・HTTP/3の実通信は未検証です。

## 利用

利用側のCMakeでは、ソースを追加するか、インストール済みパッケージを参照します。

```cmake
add_subdirectory(path/to/SwayFlux3)
target_link_libraries(my_app PRIVATE SwayFlux3::core)
# HTTPも利用する場合: SwayFlux3::http
```

独自Jobは`Job::info()`と`Job::run(JobContext)`を実装します。
[独自Jobの例](examples/custom_job.cpp)を参照してください。

- 優先度の整数値が大きいJobから実行枠を割り当て、同順位では待機要求の投入順を保ちます。
- `handle.set_priority(value)`で待機中・実行中・停止中の優先度を変更できます。
- `context.signals().priority`で高優先度Jobの有無を確認できます。枠が空いていても通知します。
- `context.pause()`は実際のI/Oを停止した後に呼びます。`resume()`がtrueを返してから処理を再開します。
- 開始済みJobのキャンセルは協調要求です。実処理を終えてから`JobOutcome::cancelled`を返します。
- `run()`は開始済みJobごとの専用スレッドで一度呼ばれ、一時停止中もスレッドを保持します。
- 終了時は`shutdown()`で既存Jobの完了を待ちます。自ら再開・終了しないJobがあれば待ち続けます。

## HTTP転送

```cpp
#include <swayflux3/swayflux3.hpp>

swayflux3::HttpTransferOptions options;
options.url = "https://example.com/data.bin";
options.path = "data.bin";
options.priority_response = swayflux3::PriorityResponse::limit_bandwidth_or_pause;
options.priority_bandwidth = 256 * 1024;
options.idle_timeout = std::chrono::seconds(60);

swayflux3::Scheduler scheduler(4);
auto handle = scheduler.enqueue(std::make_unique<swayflux3::HttpTransferJob>(options), 1);
auto result = handle.wait();
scheduler.shutdown();
```

- `sink`へ独自の`DownloadSink`を指定すると、受信チャンクを直接処理できます。全体をメモリーに保持しません。
  `write()`内でデータを消費するかコピーしてください。チャンク境界はレコード境界と一致しません。
- アップロードは`direction = upload`と原本の`path`を指定します。`upload_method`でPUT／POSTを選べます。
- 受信はgzip・deflate・Brotli・Zstd、逐次アップロード圧縮はgzip・Zstdに対応します。
  送信圧縮はサーバー側のContent-Encodingと長さ未確定の本文への対応が必要です。
- `on_response`でステータス・Content-Length・Content-Range・ETag等を確認できます。
  通常は最終ヘッダー到着時に一度通知します。認証交渉の可能性がある401／407は最終結果まで保留します。
- `range = HttpRange{offset, length}`で範囲を指定すると、identityを要求して応答の範囲・長さを検証します。
  offsetが0でContent-Lengthが要求長と一致する全体200応答も受け入れます。
- `limit_bandwidth_or_pause`は、高優先度Jobが枠を待つ間は停止し、実行中であれば帯域を制限します。
  `limit_bandwidth`だけを選んだ場合は枠を返しません。
- `total_timeout`は利用者コールバック・一時停止時間も含みます。`idle_timeout`は意図的な停止や利用者処理を除外します。
  どちらも既定0は無効です。利用者処理は強制中断せず、戻った時点で期限とキャンセルを確認します。
- Job単位のプロキシ・認証・除外指定と、外部のプロキシ解決器を接続する`ProxyResolver`を利用できます。
- TLS証明書・ホスト名検証は有効です。HTTPSからHTTPへのリダイレクトは既定で拒否し、
  独自ヘッダーとURL認証情報は最初のオリジンに限定します。
- 切断後の自動再接続、永続的な再開、PACエンジンは含みません。
  ダウンロード失敗時には途中ファイルや、Sinkへ渡した部分が残る場合があります。

[ファイル転送](examples/transfer.cpp)と[ストリーミング受信](examples/stream_download.cpp)の利用例もあります。

## インストール

```sh
cmake --install build --config Release --prefix /path/to/install
```

利用側で次を指定します。Coreのみの場合はHTTP依存ライブラリを検索しません。

```cmake
find_package(SwayFlux3 CONFIG REQUIRED COMPONENTS core)
target_link_libraries(my_app PRIVATE SwayFlux3::core)
# HTTP: find_package(SwayFlux3 CONFIG REQUIRED COMPONENTS http)
#       target_link_libraries(my_app PRIVATE SwayFlux3::http)
```

## ライセンス

本体は[0BSD](LICENSE)です。商用利用・改変・再配布が可能で、本体の著作権表示やライセンス本文の同梱、
ソースコードの公開を利用者に義務付けません。
第三者ライブラリにはそれぞれの条件が適用されます。[依存ライブラリの通知](THIRD_PARTY_NOTICES.md)を確認してください。
