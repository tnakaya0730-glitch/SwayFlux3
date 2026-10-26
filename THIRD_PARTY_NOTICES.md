# 依存ライブラリ

CoreとCommunicationはC++標準ライブラリのみで使用できます。
HTTP層とWindows用依存ビルドスクリプトでは、以下を使用します。
各ライブラリのライセンス本文を`docs/licenses/`へ収録しています。

| ライブラリ | 検証バージョン | 用途 | ライセンス本文 |
| --- | --- | --- | --- |
| [curl/libcurl](https://curl.se/) | 8.23.0 | HTTP/HTTPS・応答展開 | [curl](docs/licenses/curl.txt) |
| [zlib](https://zlib.net/) | 1.3.2 | gzip/deflate・gzip送信 | [zlib](docs/licenses/zlib.txt) |
| [Brotli](https://github.com/google/brotli) | 1.2.1 | Brotli受信展開・テスト生成 | [Brotli](docs/licenses/brotli.txt) |
| [Zstandard](https://github.com/facebook/zstd) | 1.5.7 | Zstd受信展開・送信圧縮 | [Zstandard](docs/licenses/zstd.txt) |

スクリプトによる取得は、固定したURLとSHA256で内容を確認します。
WindowsのTLSにはSchannelを使用します。HTTP/2・HTTP/3の依存ライブラリは同梱していません。
SwayFlux3本体は[0BSDライセンス](LICENSE)です。上記の第三者ライブラリには、それぞれのライセンスが適用されます。
本体の0BSDによって、第三者ライブラリの著作権表示・ライセンス表示等の条件が免除されることはありません。
