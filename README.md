# EmuZ-1500 Solaris Source

[English version](README.en.md)

このリポジトリは、TAKEDA, toshiya氏による Common Source Code Project のソースコードをベースにした、Solaris 向け EmuZ-1500 公開用ソースです。
元ソースは https://takeda-toshiya.my.coocan.jp よりダウンロードできます。

動作確認の対象は EmuZ-1500 のみです。
共通ソースツリーには他機種向けのコードが含まれていますが対応していません。

## 内容

- `src/` - Solaris向けに修正した共通エミュレータコード。
- `src/solaris/` - SDL2 と GTK2 を使用する Solaris 専用コード。
- `src/vm/mz700/` - このビルドで使用する MZ-700/MZ-1500 系 VM 実装。基本的に元ソースのままですが、`memory.cpp` には性能改善のための変更しています。
- `g++/` - GCC/GNU make 用のビルドファイル。
- `license/` - GPL および同梱されている第三者ライセンス文書。

## ビルド

`g++/Makefile.mz1500` で Solaris 版 EmuZ-1500 ホストをビルドします。
コンパイラは GCC、make は GNU make (`gmake`) を想定しています。

```sh
gmake -f g++/Makefile.mz1500
```

ビルドには OS 標準の機能のほか以下が必要です。

- C++11 対応の GCC
- GNU make (`gmake`)
- SDL2 のヘッダとライブラリ
- GTK2 のヘッダとライブラリ

生成される実行ファイルは以下です。

```sh
./mz1500
```

実行例:

```sh
./mz1500 tape-file
./mz1500 --cmt tape-file.mzt
./mz1500 --qd quick-disk-file.mzt
```

## Solaris Control UI

Windows と同じ画面構成にできなかったため、操作ウィンドウを別に設けています。
Windows の操作に準拠したサブセットのメニューを用意しています。

## ライセンス

Common Source Code Project 由来のソースコードは GNU General Public License
Version 2 or later の下で利用できます。著作権は各ソースコードの作者に帰属します。

ライセンス本文は `license/COPYING.txt` を参照してください。元プロジェクトおよび各作者による
説明は `readme.txt` と `readme_by_*.txt` を参照してください。同梱されている第三者由来コードの
ライセンス文書は `license/` 以下にあります。
