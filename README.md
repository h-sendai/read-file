# read-file

単一プロセスで同時にファイルを読むときの読み出し速度の計測。

## Usage

```
./read-file [-i] [-s | -r] [-D] [-t] [-b bufsize (64kB)] [-n total_read_size] filename
-D: ファイルを読み始めるまえにページキャッシュをドロップしない
-i: O_DIRECTでopenする(読み出しデータをキャッシュしない)
-s: posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL): readaheadサイズを通常の2倍にする
-r: posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM): readaheadを無効化する
-t: read()後の時刻を記録して、ファイル読み出し終了後time.<parent_pid>.<proc_num>ファイルに出力する。
-n: 読み出すバイト数。デフォルトはファイル全部を読む。
-b: read()のバッファサイズ。デフォルト64kB (catと同じにした）
```

[posix_fadvise(2) man page](https://man7.org/linux/man-pages/man2/posix_fadvise.2.html)

## 実装

指定されたファイルごとに1プロセスを使ってopen(), read()する。

ページキャッシュのドロップは``drop-page-cache.c``で実装している。

## bcc-tools biosnoop

bcc-toolsのセットアップ。CentOS 7, AlmaLinuxでは

```
dnf (yum) install bcc-tools
```
で``/usr/share/bcc/tools/biosnoop``に入る。

別端末で``biosnoop -d sdX``として実行するとread aheadの様子などが観察できる。
