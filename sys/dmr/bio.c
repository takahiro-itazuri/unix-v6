/*
 * Block I/O
 */

#include "../param.h"
#include "../user.h"
#include "../buf.h"
#include "../conf.h"
#include "../systm.h"
#include "../proc.h"
#include "../seg.h"

char	buffers[NBUF][514];	/* バッファデータ */
struct	buf	swbuf;		/* スワップバッファ */

/*
 * Declarations of the tables for the magtape devices;
 * see bdwrite.
 */
int	tmtab;
int	httab;

/*
 * bread 関数: 同期読み込み
 */
bread(dev, blkno)
{
	register struct buf *rbp;

	rbp = getblk(dev, blkno);
	if (rbp->b_flags&B_DONE)	/* バッファが最新だった場合 */
		return(rbp);
	rbp->b_flags =| B_READ;
	rbp->b_wcount = -256;	/* ? */
	(*bdevsw[dev.d_major].d_strategy)(rbp);
	iowait(rbp);
	return(rbp);
}

/*
 * breada 関数: 同期読み込みと先行読み込み
 */
breada(adev, blkno, rablkno)
{
	register struct buf *rbp, *rabp;
	register int dev;

	dev = adev;
	rbp = 0;
	if (!incore(dev, blkno)) {	/* ブロックのバッファが存在しない場合 */
		rbp = getblk(dev, blkno);
		if ((rbp->b_flags&B_DONE) == 0) {	/* B_DONE が立っていない場合、同期読み込み */
			rbp->b_flags =| B_READ;
			rbp->b_wcount = -256;
			(*bdevsw[adev.d_major].d_strategy)(rbp);
		}
	}
	if (rablkno && !incore(dev, rablkno)) {	/* ブロックのバッファが存在しない場合 */
		rabp = getblk(dev, rablkno);
		if (rabp->b_flags & B_DONE)
			brelse(rabp);
		else {	/* B_DONE が立っていない場合、非同期読み込み */
			rabp->b_flags =| B_READ|B_ASYNC;
			rabp->b_wcount = -256;
			(*bdevsw[adev.d_major].d_strategy)(rabp);
		}
	}
	/* ブロックのバッファが存在する場合 */
	if (rbp==0)
		return(bread(dev, blkno));
	iowait(rbp);
	return(rbp);
}

/*
 * bwrite 関数: 書き込み
 */
bwrite(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register flag;

	rbp = bp;
	flag = rbp->b_flags;
	rbp->b_flags =& ~(B_READ | B_DONE | B_ERROR | B_DELWRI);
	rbp->b_wcount = -256;
	(*bdevsw[rbp->b_dev.d_major].d_strategy)(rbp);
	if ((flag&B_ASYNC) == 0) {	/* 同期書き込み */
		iowait(rbp);
		brelse(rbp);
	} else if ((flag&B_DELWRI)==0)	/* 非同期書き込み */
		geterror(rbp);
}

/*
 * bdwrite 関数: 遅延書き込み
 */
bdwrite(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register struct devtab *dp;

	rbp = bp;
	dp = bdevsw[rbp->b_dev.d_major].d_tab;
	if (dp == &tmtab || dp == &httab)
		bawrite(rbp);
	else {
		rbp->b_flags =| B_DELWRI | B_DONE;
		brelse(rbp);
	}
}

/*
 * bawrite 関数: 非同期書き込み
 */
bawrite(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	rbp->b_flags =| B_ASYNC;
	bwrite(rbp);
}

/*
 * brelse 関数: バッファの解放
 */
brelse(bp)
struct buf *bp;
{
	register struct buf *rbp, **backp;
	register int sps;

	rbp = bp;
	if (rbp->b_flags&B_WANTED)	/* このバッファを待っているプロセスを起こす */
		wakeup(rbp);
	if (bfreelist.b_flags&B_WANTED) {	/* 利用可能なバッファを待っているプロセスを起こす */
		bfreelist.b_flags =& ~B_WANTED;
		wakeup(&bfreelist);
	}
	if (rbp->b_flags&B_ERROR)	/* エラーが発生していた場合 */
		rbp->b_dev.d_minor = -1;
	backp = &bfreelist.av_back;
	sps = PS->integ;
	spl6();
	rbp->b_flags =& ~(B_WANTED|B_BUSY|B_ASYNC);
	/* 利用可能なバッファリストの最後尾にバッファを追加する */
	(*backp)->av_forw = rbp;
	rbp->av_back = *backp;
	*backp = rbp;
	rbp->av_forw = &bfreelist;
	PS->integ = sps;
}

/*
 * incore 関数: そのブロックにバッファが割り当てられているかを確認
 */
incore(adev, blkno)
{
	register int dev;
	register struct buf *bp;
	register struct devtab *dp;

	dev = adev;
	dp = bdevsw[adev.d_major].d_tab;
	for (bp=dp->b_forw; bp != dp; bp = bp->b_forw)
		if (bp->b_blkno==blkno && bp->b_dev==dev)
			return(bp);
	return(0);
}

/*
 * getblk 関数: デバイス番号とブロック番号からバッファを取得する
 * すでに関連づけられたバッファがあれば、それを返す。B_BUSY なら B_WANTED を設定してスリープ
 * する。
 * すでに関連づけられたバッファがなければ、B_BUSY ではないバッファを探し出し、再割り当てした
 * 上で B_BUSY を設定して返す。
 */
getblk(dev, blkno)
{
	register struct buf *bp;
	register struct devtab *dp;
	extern lbolt;

	if(dev.d_major >= nblkdev)
		panic("blkdev");

    loop:
	if (dev < 0)	/* NODEV */
		dp = &bfreelist;
	else {			/* NODEV 以外 */
		dp = bdevsw[dev.d_major].d_tab;
		if(dp == NULL)
			panic("devtab");
		for (bp=dp->b_forw; bp != dp; bp = bp->b_forw) {
			if (bp->b_blkno!=blkno || bp->b_dev!=dev)
				continue;
			spl6();
			if (bp->b_flags&B_BUSY) {	/* バッファが B_BUSY な場合 */
				bp->b_flags =| B_WANTED;
				sleep(bp, PRIBIO);
				spl0();
				goto loop;
			}
			spl0();
			notavail(bp);
			return(bp);
		}
	}

	/* バッファが見つからなかった場合 */
	spl6();
	if (bfreelist.av_forw == &bfreelist) {	/* 利用可能なバッファリストが空の場合 */
		bfreelist.b_flags =| B_WANTED;
		sleep(&bfreelist, PRIBIO);
		spl0();
		goto loop;
	}
	spl0();
	notavail(bp = bfreelist.av_forw);
	if (bp->b_flags & B_DELWRI) {	/* 遅延書き込みフラグが設定されている場合 */
		bp->b_flags =| B_ASYNC;
		bwrite(bp);
		goto loop;
	}
	bp->b_flags = B_BUSY | B_RELOC;
	bp->b_back->b_forw = bp->b_forw;
	bp->b_forw->b_back = bp->b_back;
	bp->b_forw = dp->b_forw;
	bp->b_back = dp;
	dp->b_forw->b_back = bp;
	dp->b_forw = bp;
	bp->b_dev = dev;
	bp->b_blkno = blkno;
	return(bp);
}

/*
 * iowait 関数: I/O の完了を待つ
 */
iowait(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	spl6();
	while ((rbp->b_flags&B_DONE)==0)	/* B_DONE フラグが立つまで待つ */
		sleep(rbp, PRIBIO);
	spl0();
	geterror(rbp);
}

/*
 * notavail 関数: 利用可能なバッファからバッファを取得する
 * 利用可能なバッファリストからバッファを切り離し、B_BUSY を設定する。
 */
notavail(bp)
struct buf *bp;
{
	register struct buf *rbp;
	register int sps;

	rbp = bp;
	sps = PS->integ;
	spl6();
	rbp->av_back->av_forw = rbp->av_forw;
	rbp->av_forw->av_back = rbp->av_back;
	rbp->b_flags =| B_BUSY;
	PS->integ = sps;
}

/*
 * iodone 関数: I/O が完了したことをマークする
 */
iodone(bp)
struct buf *bp;
{
	register struct buf *rbp;

	rbp = bp;
	if(rbp->b_flags&B_MAP)
		mapfree(rbp);
	rbp->b_flags =| B_DONE;
	if (rbp->b_flags&B_ASYNC)	/* 非同期 I/O */
		brelse(rbp);
	else {				/* 同期 I/O */
		rbp->b_flags =& ~B_WANTED;
		wakeup(rbp);
	}
}

/*
 * clrbuf 関数: バッファデータの 0 クリア
 */
clrbuf(bp)
int *bp;	/* struct buf* bp; しない理由がわからない */
{
	register *p;
	register c;

	p = bp->b_addr;
	c = 256;
	do
		*p++ = 0;
	while (--c);
}

/*
 * binit 関数: バッファ I/O システムの初期化
 * ・bfreelist を初期化する
 * ・buf と buffers を紐付ける
 * ・すべてのバッファを NODEV の b_list に追加する。
 * ・bdevsw に設定されている devtab.d_tab を初期化する。
 */
binit()
{
	register struct buf *bp;
	register struct devtab *dp;
	register int i;
	struct bdevsw *bdp;

	bfreelist.b_forw = bfreelist.b_back =
	    bfreelist.av_forw = bfreelist.av_back = &bfreelist;
	for (i=0; i<NBUF; i++) {
		bp = &buf[i];
		bp->b_dev = -1;			/* -1: NODEV */
		bp->b_addr = buffers[i];	/* buf と buffers の紐付け */
		/* bfreelist の b_list の先頭に挿入する */
		bp->b_back = &bfreelist;
		bp->b_forw = bfreelist.b_forw;
		bfreelist.b_forw->b_back = bp;
		bfreelist.b_forw = bp;
		bp->b_flags = B_BUSY;
		brelse(bp);
	}
	i = 0;
	/* bdevsw の初期化 */
	for (bdp = bdevsw; bdp->d_open; bdp++) {
		dp = bdp->d_tab;
		if(dp) {
			dp->b_forw = dp;
			dp->b_back = dp;
		}
		i++;
	}
	nblkdev = i;
}

/*
 * Device start routine for disks
 * and other devices that have the register
 * layout of the older DEC controllers (RF, RK, RP, TM)
 */
#define	IENABLE	0100
#define	WCOM	02
#define	RCOM	04
#define	GO	01
devstart(bp, devloc, devblk, hbcom)
struct buf *bp;
int *devloc;
{
	register int *dp;
	register struct buf *rbp;
	register int com;

	dp = devloc;
	rbp = bp;
	*dp = devblk;			/* block address */
	*--dp = rbp->b_addr;		/* buffer address */
	*--dp = rbp->b_wcount;		/* word count */
	com = (hbcom<<8) | IENABLE | GO |
		((rbp->b_xmem & 03) << 4);
	if (rbp->b_flags&B_READ)	/* command + x-mem */
		com =| RCOM;
	else
		com =| WCOM;
	*--dp = com;
}

/*
 * startup routine for RH controllers.
 */
#define	RHWCOM	060
#define	RHRCOM	070

rhstart(bp, devloc, devblk, abae)
struct buf *bp;
int *devloc, *abae;
{
	register int *dp;
	register struct buf *rbp;
	register int com;

	dp = devloc;
	rbp = bp;
	if(cputype == 70)
		*abae = rbp->b_xmem;
	*dp = devblk;			/* block address */
	*--dp = rbp->b_addr;		/* buffer address */
	*--dp = rbp->b_wcount;		/* word count */
	com = IENABLE | GO |
		((rbp->b_xmem & 03) << 8);
	if (rbp->b_flags&B_READ)	/* command + x-mem */
		com =| RHRCOM; else
		com =| RHWCOM;
	*--dp = com;
}

/*
 * 11/70 routine to allocate the
 * UNIBUS map and initialize for
 * a unibus device.
 * The code here and in
 * rhstart assumes that an rh on an 11/70
 * is an rh70 and contains 22 bit addressing.
 */
int	maplock;
mapalloc(abp)
struct buf *abp;
{
	register i, a;
	register struct buf *bp;

	if(cputype != 70)
		return;
	spl6();
	while(maplock&B_BUSY) {
		maplock =| B_WANTED;
		sleep(&maplock, PSWP);
	}
	maplock =| B_BUSY;
	spl0();
	bp = abp;
	bp->b_flags =| B_MAP;
	a = bp->b_xmem;
	for(i=16; i<32; i=+2)
		UBMAP->r[i+1] = a;
	for(a++; i<48; i=+2)
		UBMAP->r[i+1] = a;
	bp->b_xmem = 1;
}

mapfree(bp)
struct buf *bp;
{

	bp->b_flags =& ~B_MAP;
	if(maplock&B_WANTED)
		wakeup(&maplock);
	maplock = 0;
}

/*
 * swap I/O
 */
swap(blkno, coreaddr, count, rdflg)
{
	register int *fp;

	fp = &swbuf.b_flags;
	spl6();
	while (*fp&B_BUSY) {
		*fp =| B_WANTED;
		sleep(fp, PSWP);
	}
	*fp = B_BUSY | B_PHYS | rdflg;
	swbuf.b_dev = swapdev;
	swbuf.b_wcount = - (count<<5);	/* 32 w/block */
	swbuf.b_blkno = blkno;
	swbuf.b_addr = coreaddr<<6;	/* 64 b/block */
	swbuf.b_xmem = (coreaddr>>10) & 077;
	(*bdevsw[swapdev>>8].d_strategy)(&swbuf);
	spl6();
	while((*fp&B_DONE)==0)
		sleep(fp, PSWP);
	if (*fp&B_WANTED)
		wakeup(fp);
	spl0();
	*fp =& ~(B_BUSY|B_WANTED);
	return(*fp&B_ERROR);
}

/*
 * make sure all write-behind blocks
 * on dev (or NODEV for all)
 * are flushed out.
 * (from umount and update)
 */
/*
 * bflush 関数: 遅延書き込みバッファをまとめて書き出す
 */
bflush(dev)
{
	register struct buf *bp;

loop:
	spl6();
	for (bp = bfreelist.av_forw; bp != &bfreelist; bp = bp->av_forw) {
		if (bp->b_flags&B_DELWRI && (dev == NODEV||dev==bp->b_dev)) {
			bp->b_flags =| B_ASYNC;
			notavail(bp);
			bwrite(bp);
			goto loop;
		}
	}
	spl0();
}

/*
 * physio 関数: RAW I/O
 */
physio(strat, abp, dev, rw)
struct buf *abp;
int (*strat)();
{
	register struct buf *bp;
	register char *base;
	register int nb;
	int ts;

	bp = abp;
	base = u.u_base;	/* 転送データの仮想アドレス */
	/*
	 * Check odd base, odd count, and address wraparound
	 */
	if (base&01 || u.u_count&01 || base>=base+u.u_count)
		goto bad;
	ts = (u.u_tsize+127) & ~0177;
	if (u.u_sep)
		ts = 0;
	nb = (base>>6) & 01777;
	/*
	 * Check overlap with text. (ts and nb now
	 * in 64-byte clicks)
	 */
	if (nb < ts)
		goto bad;
	/*
	 * Check that transfer is either entirely in the
	 * data or in the stack: that is, either
	 * the end is in the data or the start is in the stack
	 * (remember wraparound was already checked).
	 */
	if ((((base+u.u_count)>>6)&01777) >= ts+u.u_dsize
	    && nb < 1024-u.u_ssize)
		goto bad;
	spl6();
	while (bp->b_flags&B_BUSY) {
		bp->b_flags =| B_WANTED;
		sleep(bp, PRIBIO);
	}
	bp->b_flags = B_BUSY | B_PHYS | rw;
	bp->b_dev = dev;
	/*
	 * Compute physical address by simulating
	 * the segmentation hardware.
	 */
	bp->b_addr = base&077;
	base = (u.u_sep? UDSA: UISA)->r[nb>>7] + (nb&0177);
	bp->b_addr =+ base<<6;
	bp->b_xmem = (base>>10) & 077;
	bp->b_blkno = lshift(u.u_offset, -9);
	bp->b_wcount = -((u.u_count>>1) & 077777);
	bp->b_error = 0;
	u.u_procp->p_flag =| SLOCK;
	(*strat)(bp);
	spl6();
	while ((bp->b_flags&B_DONE) == 0)
		sleep(bp, PRIBIO);
	u.u_procp->p_flag =& ~SLOCK;
	if (bp->b_flags&B_WANTED)
		wakeup(bp);
	spl0();
	bp->b_flags =& ~(B_BUSY|B_WANTED);
	u.u_count = (-bp->b_resid)<<1;
	geterror(bp);
	return;
    bad:
	u.u_error = EFAULT;
}

/*
 * geterror 関数: エラー処理
 */
geterror(abp)
struct buf *abp;
{
	register struct buf *bp;

	bp = abp;
	if (bp->b_flags&B_ERROR)
		if ((u.u_error = bp->b_error)==0)
			u.u_error = EIO;
}
