# EmuZ-1500 Solaris Source

[English version](README.md)

このリポジトリは、武田俊也氏による Common Source Code Project のソースコードを
ベースにした、Solaris 向け EmuZ-1500 公開用ソースです。元ソースは 
https://takeda-toshiya.my.coocan.jp よりダウンロードできます。

このリポジトリでビルドと動作確認の対象にしているのは Solaris 上の EmuZ-1500 のみです。
共通ソースツリーには他機種向けのコードが含まれる場合がありますが、このリポジトリは
EmuZ-1500 の Solaris ビルドとして扱ってください。

## 内容

- `src/` - Solaris 版 EmuZ-1500 のビルドに必要な共通エミュレータコード、仮想マシンデバイス、ホストコード。
- `src/solaris/` - SDL2 と GTK2 を使用する Solaris 直接起動ホスト。
- `src/vm/mz700/` - このビルドで使用する MZ-700/MZ-1500 系 VM 実装。基本的に元ソースのままですが、`memory.cpp` には性能改善のための変更を入れています。
- `license/` - GPL および同梱されている第三者ライセンス文書。

## ビルド

リポジトリ直下の `Makefile` で Solaris 版 EmuZ-1500 ホストをビルドします。
コンパイラは GCC、make は GNU make (`gmake`) を想定しています。

```sh
gmake
```

Solaris ビルドには以下が必要です。

- C++11 対応の GCC
- GNU make (`gmake`)
- SDL2 のヘッダとライブラリ
- GTK2 のヘッダとライブラリ

生成される実行ファイルは以下です。

```sh
./mz1500
```

イメージファイルを指定して起動する例:

```sh
./mz1500 tape-file
./mz1500 --cmt tape-file
./mz1500 --qd quick-disk-file
```

## Solaris Control UI
Windowsと同じ画面構成にできなかったため、操作ウィンドウを別に設けています。
Windowsの操作に準拠したサブセットのメニューを用意しています。

リリースビルドでは console からのコマンド入力は無効です。診断用に使用したい場合は、
`-DDEBUG` を付けてビルドすると stdin からのコマンド入力が有効になります。

```sh
env CXXFLAGS=-DDEBUG gmake
```

DEBUG ビルドでは、stdin から `help`, `status`, `cmt`, `cmtrec`, `cmteject`,
`qd`, `qdeject`, `option`, `reset`, `exit` などのコマンドを入力できます。

## ライセンス

このソースコードは GNU General Public License Version 2 or later の下で利用できます。
ライセンス本文は `license/COPYING.txt` を参照してください。同梱されている第三者ライセンスは
`license/` 以下にあります。

## GitHub へ公開する際の注意

- `readme.txt` には元のプロジェクト説明と謝辞が含まれているため、残してください。
- 公開時は `license/` ディレクトリ全体を含めてください。
- このリポジトリは元ソース全体の upstream mirror ではなく、Solaris 版 EmuZ-1500 の公開用ソースであることを明記してください。
- 生成されたバイナリやローカルのオブジェクトファイルは、リリースパッケージに意図して含める場合を除き、公開しないでください。
