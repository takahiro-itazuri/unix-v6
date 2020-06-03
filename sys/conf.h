/*
 * Used to dissect integer device code
 * into major (driver designation) and
 * minor (driver parameter) parts.
 */
struct
{
	char	d_minor;
	char	d_major;
};

/*
 * bdevsw 構造体: ブロックデバイスドライバ
 */
struct	bdevsw
{
	int	(*d_open)();		/* オープン関数へのポインタ */
	int	(*d_close)();		/* クローズ関数へのポインタ */
	int	(*d_strategy)();	/* アクセス関数へのポインタ */
	int	*d_tab;			/* devtab 構造体へのポインタ */
} bdevsw[];

/*
 * nblkdev: ブロックデバイスの個数
 */
int	nblkdev;

/*
 * cdevsw 構造体: キャラクタデバイスドライバ
 */
struct	cdevsw
{
	int	(*d_open)();
	int	(*d_close)();
	int	(*d_read)();
	int	(*d_write)();
	int	(*d_sgtty)();
} cdevsw[];

/*
 * nchrdev: キャラクタデバイスの個数
 */
int	nchrdev;
