/*
 * buf 構造体: ブロックデバイスのバッファヘッダ (バッファの管理情報)
 * 任意のバッファは、2 つの双方向リスト (b_forw/b_back と av_forw/av_back) でリンクされている。
 * b_forw/b_back は、各ブロックデバイスに割り当てられているバッファを表す。
 * av_forw/av_back は、処理中 (B_BUSY) でないバッファが、最長不使用 (LRU: Least Recently Used) の順番
 * で並んでおり、新しいバッファの割り当てに利用される。
 * 任意のバッファが b-list 内に存在しているが、av-list に存在しているとは限らない。
 */
struct buf
{
	int	b_flags;		/* フラグ (下部に定義あり) */
	struct	buf *b_forw;		/* b-list の前方 */
	struct	buf *b_back;		/* b-list の後方 */
	struct	buf *av_forw;		/* av-list の前方 */
	struct	buf *av_back;		/* av-list の後方 */
	int	b_dev;			/* デバイス番号 (メジャー番号+マイナー番号) */
	int	b_wcount;		/* 読み書きサイズ */
	char	*b_addr;		/* メモリ中のバッファデータへのポインタ */
	char	*b_xmem;		/* b_addr の拡張ビット */
	char	*b_blkno;		/* ブロック番号 */
	char	b_error;		/* I/O でエラーが発生した場合に使用する */
	char	*b_resid;		/* エラーによって転送できなかったデータ量 */
} buf[NBUF];				/* NBUF は sys/param.h で定義されている */

/*
 * devtab 構造体: 各ブロックデバイスごとに用意される構造体
 * 2 つの双方向リスト (b_forw/b_back と d_actf/d_actl) を保持している。
 * b_forw/b_back は、このデバイスのバッファの先頭と最後尾を示している。
 * d_actf/d_actl は、デバイスの I/O キューの先頭と最後尾を示している。
 */
struct devtab
{
	char	d_active;		/* デバイスの処理中 */
	char	d_errcnt;		/* エラーカウント */
	struct	buf *b_forw;		/* バッファの先頭 */
	struct	buf *b_back;		/* バッファの最後尾 */
	struct	buf *d_actf;		/* I/O キューの先頭 */
	struct 	buf *d_actl;		/* I/O キューの最後尾 */
};

struct	buf bfreelist; /* デバイスに割り当てられていない (NODEV の) バッファの先頭 */


/*
 * バッファのフラグ
 * buf 構造体の b_flags で使用する。
 */
#define	B_WRITE	0	/* 書き出し */
#define	B_READ	01	/* 読み込み */
#define	B_DONE	02	/* 最新の情報が保持されている */
#define	B_ERROR	04	/* エラーが発生した */
#define	B_BUSY	010	/* バッファは使用中で、av_forw/av_back 上にない */
#define	B_PHYS	020	/* RAW 入出力中 */
#define	B_MAP	040	/* UNIBUS マップ (PDP-11/40 では使用しない) */
#define	B_WANTED 0100	/* バッファは使用中であり、解放されるのを待っているプロセスがある */
#define	B_RELOC	0200	/* このフラグは使用されていない */
#define	B_ASYNC	0400	/* 先行読み込み、非同期書き出しを行う */
#define	B_DELWRI 01000	/* 遅延書き込み */
