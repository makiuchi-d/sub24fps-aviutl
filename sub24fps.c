/*******************************************************************************
* 	����24fps(�㗝)�t�B���^
* 								ver. 0.01
* 
* [2005]
* 	02/22:	������Ǝg�������Ȃ��č���Ă݂�B
* 	02/23:	�ςȂƂ�������������悤�Ƃ����킯�����A�悭�킩���B
* 	      	�Ƃ肠�����]����������x�Ƃ��ă|�C���^��n���Ă݂�B
* 	      	�ꉞ�������B������AviUtl���̐ݒ莟��ŗ�����B
* 	      	���ő�摜�T�C�Y ��856x576 �~720x480
* 	02/24:	���߁B����グ�B
* 	      	�J�������Ƃ������Ă����Ȃ��炢���Ȃ�X�V�B
* 	      	�Ƃ肠�������̃o�O�͎�ꂽ�B0.98d,0.99�ł͐���쓮���m�F�B
* 	03/07:	�\����������ʂ芮���B
* 	      	0.97f�ȑO�ł͎���24fps��func_proc�������B
* 	      	get_ycp_filtering_cache���g����̂�0.98�ȍ~�B(set_�c�����l)
* 	      	get_ycp_filtering_cache_ex���g����̂�0.98d�ȍ~�B
* 	      	0.98b�ȑO�ł�get_sys_info��editp��NULL��n���Ȃ��B
* 	      	�cSYS_INFO���Ȃ��ƃo�[�W�����`�F�b�N�ł��Ȃ������orz
* 			0.98c�ȍ~�Ő���쓮�m�F�A���J(0.01)
* 
*******************************************************************************/
#include <windows.h>
#include "filter.h"


//----------------------------
//	FILTER_DLL�\����
//----------------------------
#define track_N 2
#if track_N
TCHAR *track_name[]   = { "�������l","�͈�" };	// �g���b�N�o�[�̖��O
int   track_default[] = {  64,  16 };	// �g���b�N�o�[�̏����l
int   track_s[]       = {   0,   0 };	// �g���b�N�o�[�̉����l
int   track_e[]       = { 256, 256 };	// �g���b�N�o�[�̏���l
#endif

#define check_N 2
#if check_N
TCHAR *check_name[]   = { "���ȕ������d��",
						  "�h�b�g�P�ʉ���" };	// �`�F�b�N�{�b�N�X
int   check_default[] = { 0, 0 };	// �f�t�H���g
#endif


FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	NULL,NULL,			// �ݒ�E�C���h�E�̃T�C�Y
	"����24fps(�㗝)",		// �t�B���^�̖��O
	track_N,        	// �g���b�N�o�[�̐�
#if track_N
	track_name,     	// �g���b�N�o�[�̖��O�S
	track_default,  	// �g���b�N�o�[�̏����l�S
	track_s,track_e,	// �g���b�N�o�[�̐��l�̉������
#else
	NULL,NULL,NULL,NULL,
#endif
	check_N,      	// �`�F�b�N�{�b�N�X�̐�
#if check_N
	check_name,   	// �`�F�b�N�{�b�N�X�̖��O�S
	check_default,	// �`�F�b�N�{�b�N�X�̏����l�S
#else
	NULL,NULL,
#endif
	func_proc,   		// �t�B���^�����֐�
	func_init,
	func_exit,   		// �J�n��,�I�����ɌĂ΂��֐�
	NULL,        		// �ݒ肪�ύX���ꂽ�Ƃ��ɌĂ΂��֐�
	func_WndProc,		// �ݒ�E�B���h�E�v���V�[�W��
	NULL,NULL,   		// �V�X�e���Ŏg�p
	NULL,NULL,   		// �g���f�[�^�̈�
	"����24fps(�㗝) ver 0.01 by MakKi",	// �t�B���^���
	NULL,			// �Z�[�u�J�n���O�ɌĂ΂��֐�
	NULL,			// �Z�[�u�I�����ɌĂ΂��֐�
	NULL,NULL,NULL,	// �V�X�e���Ŏg�p
	NULL,			// �g���̈揉���l
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
*	�L���b�V��
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
*	�L���b�V��������
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
*	�L���b�V���j��
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
*	�L���b�V������
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
*	EXFUNC�[���֐�
*-----------------------------------------------------------------------------*/
void *emu_get_ycp_source_cache(void *editp,int n,int ofs)
{
	int w,h;
	void *cache;
	PIXEL_YC *src;
	PIXEL_YC *p;
	int rowsize;
	int pitch;

	cache_serch(&frame_cache,0,&cache);	//����������Ȃ��̂œ����̂������Ă�����

	if(thisfp->exfunc->get_ycp_filtering_cache_ex){
		src = thisfp->exfunc->get_ycp_filtering_cache_ex(thisfp,editp,n+ofs,&w,&h);
	}
	else {
		src = thisfp->exfunc->get_ycp_filtering_cache(thisfp,editp,n+ofs);
		w = max_w;
		h = max_h;
	}

	// ����24fps���������̈�̑O�̂ق����g��������̂�
	// �]�T���������ăL���b�V���ɃR�s�[
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
*	�t�B���^�����֐�
*=============================================================================*/
BOOL func_proc(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	BOOL r;
	EXFUNC *p;

	if(!auto24fps){
		return FALSE;
	}

	thisfp = fp;

	// EXFUNC��u�������ČĂяo��
	p = fp->exfunc;
	fp->exfunc = &exfunc;

	r = auto24fps->func_proc(fp,fpip);

	fp->exfunc = p;

	return r;
}

/*==============================================================================
*	�J�n���ɌĂ΂��֐�
*=============================================================================*/
BOOL func_init( FILTER *fp )
{
	int i;
	SYS_INFO si;

	fp->exfunc->get_sys_info(NULL,&si);

	// �T��
	for(i=0;i<si.filter_n;i++){
		auto24fps = fp->exfunc->get_filterp(i);
		if(auto24fps){
			if(lstrcmp(auto24fps->name,"����24fps")==0) break;
		}
	}
	if(auto24fps==NULL){
		MessageBox(fp->hwnd,"����24fps��������܂���",fp->name,MB_OK);
		return FALSE;
	}

	// �[��EXFUNC
	exfunc = *auto24fps->exfunc;
	exfunc.get_ycp_source_cache = emu_get_ycp_source_cache;

	max_w = si.vram_w;
	max_h = si.vram_h;

	// AviUtl���L���b�V��������
	fp->exfunc->set_ycp_filtering_cache_size(fp,max_w,max_h,10,NULL);

	// �Ǝ��L���b�V��������
	if(!init_cache(&frame_cache,5,max_w*(max_h+MARGINE*2)*sizeof(PIXEL_YC))){
		MessageBox(fp->hwnd,"�L���b�V���m�ێ��s",fp->name,MB_OK);
		auto24fps = NULL;
	}
	margine = (max_w * MARGINE * sizeof(PIXEL_YC) + 31)&~31;

	return !!auto24fps;
}

/*==============================================================================
*	�I�����ɌĂ΂��֐�
*=============================================================================*/
BOOL func_exit( FILTER *fp )
{
	cache_exit(&frame_cache);
	return TRUE;
}

/*==============================================================================
*	�ݒ�E�B���h�E�v���V�[�W��
*=============================================================================*/
BOOL func_WndProc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp )
{
	switch(message){
		case WM_KEYUP:	// ���C���E�B���h�E�֑���
		case WM_KEYDOWN:
		case WM_MOUSEWHEEL:
			SendMessage(GetWindow(hwnd, GW_OWNER), message, wparam, lparam);
			break;
	}

	return FALSE;
}
