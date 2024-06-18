# read-file

ファイルを読むときの読み出し速度の計測。

read()するまえにページキャッシュを消している（drop-page-cache.c）

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

## fdisk

```console
/dev/sde (HGST HDN724040ALE640 4TB)

sde1 200G 
sde2 200G

% sudo fdisk -l /dev/sde
Disk /dev/sde: 3.64 TiB, 4000787030016 bytes, 7814037168 sectors
Disk model: HGST HDN724040AL
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 4096 bytes
I/O size (minimum/optimal): 4096 bytes / 4096 bytes
Disklabel type: gpt
Disk identifier: D6CEB9AB-1857-4C70-A353-D6174058119E

Device         Start       End   Sectors  Size Type
/dev/sde1       2048 419432447 419430400  200G Linux filesystem
/dev/sde2  419432448 838862847 419430400  200G Linux filesystem

## mkfs

mkfs.xfs /dev/sde1

mkfs.ext4 /dev/sde2

## blockdev、/sys/block/sde/queue/read_ahead_kbでreadaheadサイズを確認

% sudo blockdev --report /dev/sde1
RO    RA   SSZ   BSZ        StartSec            Size   Device
rw   256   512  4096            2048    214748364800   /dev/sde1
% sudo blockdev --report /dev/sde2
RO    RA   SSZ   BSZ        StartSec            Size   Device
rw   256   512  4096       419432448    214748364800   /dev/sde2
% cat /sys/block/sde/queue/read_ahead_kb
128
```

sde1, sde2とも128kBである。

sde1(xfs), xde2(ext4)でそれぞれ1GBのファイルをddで作成
```
dd if=/dev/urandom of=test.file.1 bs=1024k count=1024
```
等。

## read

デフォルトとfadvise(POSIX_SEQUENTIAL)で64MBファイルを読んでみる。

```console
% ./read-file -n 64m /xfs/sendai/test.file.1
169.832 MB/s 67108864 bytes 0.376844 sec /xfs/sendai/test.file.1
% ./read-file -s -n 64m /xfs/sendai/test.file.2
168.038 MB/s 67108864 bytes 0.380866 sec /xfs/sendai/test.file.2
% ./read-file -s -n 64m /ext4/sendai/test.file.2
157.850 MB/s 67108864 bytes 0.405449 sec /ext4/sendai/test.file.2
% ./read-file -n 64m /ext4/sendai/test.file.1
163.585 MB/s 67108864 bytes 0.391233 sec /ext4/sendai/test.file.1
```

読んだあとfincoreでページキャッシュの量を確認。

```
% fincore -b /xfs/sendai/* /ext4/sendai/*
     RES PAGES       SIZE FILE
67239936 16416 1073741824 /xfs/sendai/test.file.1
67502080 16480 1073741824 /xfs/sendai/test.file.2
67239936 16416 1073741824 /ext4/sendai/test.file.1
67502080 16480 1073741824 /ext4/sendai/test.file.2
```

- 64MB(readした分)+128kB(read ahead(POSIX_NORMAL)) = 67239936
- 64MB(readした分)+128kB*2(read ahead(POSIX_SEQUENTIAL)でPOSIX_NORMALの2倍) = 67371008

67502080 = 64MB+128kB*3

## bcc-tools biosnoop

bcc-toolsのセットアップ。CentOS 7, AlmaLinuxでは

```
dnf (yum) install bcc-tools
```
で``/usr/share/bcc/tools/biosnoop``に入る。

別端末で``biosnoop -d sdX``として実行するとread aheadの様子などが観察できる。

