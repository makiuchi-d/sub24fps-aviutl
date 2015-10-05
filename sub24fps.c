/*******************************************************************************
* 	自動24fps(代理)フィルタ
* 								ver. 0.01
* 
* [2005]
* 	02/22:	ちょっと使いたくなって作ってみる。
* 	02/23:	変なところを書き換えようとされるわけだが、よくわからん。
* 	      	とりあえず余白をある程度とってポインタを渡してみる。
* 	      	一応動いた。ただしAviUtl側の設定次第で落ちる。
* 	      	→最大画像サイズ ○856x576 ×720x480
* 	02/24:	だめ。お手上げ。
* 	      	開発放棄とか言っておきながらいきなり更新。
* 	      	とりあえず↑のバグは取れた。0.98d,0.99では正常作動を確認。
* 	03/07:	予備調査も一通り完了。
* 	      	0.97f以前では自動24fpsにfunc_procが無い。
* 	      	get_ycp_filtering_cacheが使えるのは0.98以降。(set_…も同様)
* 	      	get_ycp_filtering_cache_exが使えるのは0.98d以降。
* 	      	0.98b以前ではget_sys_infoのeditpにNULLを渡せない。
* 	      	…SYS_INFO取れないとバージョンチェックできないじゃんorz
* 			0.98c以降で正常作動確認、公開(0.01)
* 
*******************************************************************************/
#include <windows.h>
#include "filter.h"


//----------------------------
//	FILTER_DLL構造体
//----------------------------
#define track_N 2
#if track_N
TCHAR *track_name[]   = { "しきい値","範囲" };	// トラックバーの名前
int   track_default[] = {  64,  16 };	// トラックバーの初期値
int   track_s[]       = {   0,   0 };	// トラックバーの下限値
int   track_e[]       = { 256, 256 };	// トラックバーの上限値
#endif

#define check_N 2
#if check_N
TCHAR *check_name[]   = { "横縞部分を二重化",
						  "ドット単位解除" };	// チェックボックス
int   check_default[] = { 0, 0 };	// デフォルト
#endif


FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	NULL,NULL,			// 設定ウインドウのサイズ
	"自動24fps(代理)",		// フィルタの名前
	track_N,        	// トラックバーの数
#if track_N
	track_name,     	// トラックバーの名前郡
	track_default,  	// トラックバーの初期値郡
	track_s,track_e,	// トラックバーの数値の下限上限
#else
	NULL,NULL,NULL,NULL,
#endif
	check_N,      	// チェックボックスの数
#if check_N
	check_name,   	// チェックボックスの名前郡
	check_default,	// チェックボックスの初期値郡
#else
	NULL,NULL,
#endif
	func_proc,   		// フィルタ処理関数
	func_init,
	func_exit,   		// 開始時,終了時に呼ばれる関数
	NULL,        		// 設定が変更されたときに呼ばれる関数
	func_WndProc,		// 設定ウィンドウプロシージャ
	NULL,NULL,   		// システムで使用
	NULL,NULL,   		// 拡張データ領域
	"自動24fps(代理) ver 0.01 by MakKi",	// フィルタ情報
	NULL,			// セーブ開始直前に呼ばれる関数
	NULL,			// セーブ終了時に呼ばれる関数
	NULL,NULL,NULL,	// システムで使用
	NULL,			// 拡張領域初期値
};

/*******************************************************************************
*	DLL Export
*******************************************************************************/
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable( void )
{
	return &filter;
}



FILTER *auto24fps;
FILTER *thisfp;
EXFUNC exfunc;
#define MARGINE 3
int max_w,max_h;
int margine;

/*------------------------------------------------------------------------------
*	キャッシュ
*-----------------------------------------------------------------------------*/
typedef struct _CACHE{
	struct {
		void *base;
		void *ptr;
		long id;
	} *node;
	int num;
	int cur;
} CACHE;

static CACHE frame_cache;

/*------------------------------------------------------------------------------
*	キャッシュ初期化
*-----------------------------------------------------------------------------*/
static BOOL init_cache(CACHE *c,int num,size_t size)
{
	int i;

	c->node = malloc(num * sizeof(*c->node));
	if(!c->node) return FALSE;

	c->num = 0;
	for(i=num;i;--i){
		void *p = malloc(size+32);
		if(!p) continue;

		c->node[c->num].base = p;
		(ULONG)c->node[c->num].ptr  = ((ULONG)p+31)&~31;
		c->node[c->num].id = 0;
		++(c->num);
	}

	c->cur = 0;

	return (c->num)?TRUE:FALSE;
}

/*------------------------------------------------------------------------------
*	キャッシュ破棄
*-----------------------------------------------------------------------------*/
static void cache_exit(CACHE *c)
{
	int i;
	if(c->node){
		for(i=c->num;i;--i){
			if(c->node[i-1].base) free(c->node[i-1].base);
		}
		free(c->node);
	}
	c->node = NULL;
	c->num = 0;
}

/*------------------------------------------------------------------------------
*	キャッシュ検索
*-----------------------------------------------------------------------------*/
static BOOL cache_serch(CACHE *c,long id,void **pp)
{
	int i;
	if(id){
		for(i=0;i<c->num;i++){
			if(c->node[i].id==id){
				*pp = c->node[i].ptr;
				return TRUE;
			}
		}
	}

	c->cur += 1;
	if(c->cur>=c->num) c->cur = 0;

	c->node[c->cur].id = id;
	*pp = c->node[c->cur].ptr;
	return FALSE;
}



/*------------------------------------------------------------------------------
*	EXFUNC擬似関数
*-----------------------------------------------------------------------------*/
void *emu_get_ycp_source_cache(void *editp,int n,int ofs)
{
	int w,h;
	void *cache;
	PIXEL_YC *src;
	PIXEL_YC *p;
	int rowsize;
	int pitch;

	cache_serch(&frame_cache,0,&cache);	//高速化じゃないので同じのがあっても無視

	if(thisfp->exfunc->get_ycp_filtering_cache_ex){
		src = thisfp->exfunc->get_ycp_filtering_cache_ex(thisfp,editp,n+ofs,&w,&h);
	}
	else {
		src = thisfp->exfunc->get_ycp_filtering_cache(thisfp,editp,n+ofs);
		w = max_w;
		h = max_h;
	}

	// 自動24fpsがメモリ領域の前のほうを使いたがるので
	// 余裕を持たせてキャッシュにコピー
	cache = (void *)((ULONG)cache + margine);
	p = cache;

	rowsize = w * sizeof(PIXEL_YC);

	for(;h;--h){
		memcpy(p,src,rowsize);
		p += max_w;
		src += max_w;
	}

	return cache;
}


/*==============================================================================
*	フィルタ処理関数
*=============================================================================*/
BOOL func_proc(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	BOOL r;
	EXFUNC *p;

	if(!auto24fps){
		return FALSE;
	}

	thisfp = fp;

	// EXFUNCを置き換えて呼び出し
	p = fp->exfunc;
	fp->exfunc = &exfunc;

	r = auto24fps->func_proc(fp,fpip);

	fp->exfunc = p;

	return r;
}

/*==============================================================================
*	開始時に呼ばれる関数
*=============================================================================*/
BOOL func_init( FILTER *fp )
{
	int i;
	SYS_INFO si;

	fp->exfunc->get_sys_info(NULL,&si);

	// 探す
	for(i=0;i<si.filter_n;i++){
		auto24fps = fp->exfunc->get_filterp(i);
		if(auto24fps){
			if(lstrcmp(auto24fps->name,"自動24fps")==0) break;
		}
	}
	if(auto24fps==NULL){
		MessageBox(fp->hwnd,"自動24fpsが見つかりません",fp->name,MB_OK);
		return FALSE;
	}

	// 擬似EXFUNC
	exfunc = *auto24fps->exfunc;
	exfunc.get_ycp_source_cache = emu_get_ycp_source_cache;

	max_w = si.vram_w;
	max_h = si.vram_h;

	// AviUtl側キャッシュ初期化
	fp->exfunc->set_ycp_filtering_cache_size(fp,max_w,max_h,10,NULL);

	// 独自キャッシュ初期化
	if(!init_cache(&frame_cache,5,max_w*(max_h+MARGINE*2)*sizeof(PIXEL_YC))){
		MessageBox(fp->hwnd,"キャッシュ確保失敗",fp->name,MB_OK);
		auto24fps = NULL;
	}
	margine = (max_w * MARGINE * sizeof(PIXEL_YC) + 31)&~31;

	return !!auto24fps;
}

/*==============================================================================
*	終了時に呼ばれる関数
*=============================================================================*/
BOOL func_exit( FILTER *fp )
{
	cache_exit(&frame_cache);
	return TRUE;
}

/*==============================================================================
*	設定ウィンドウプロシージャ
*=============================================================================*/
BOOL func_WndProc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp )
{
	switch(message){
		case WM_KEYUP:	// メインウィンドウへ送る
		case WM_KEYDOWN:
		case WM_MOUSEWHEEL:
			SendMessage(GetWindow(hwnd, GW_OWNER), message, wparam, lparam);
			break;
	}

	return FALSE;
}
