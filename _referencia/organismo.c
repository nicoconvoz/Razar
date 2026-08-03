/* =============================================================================
 *  ORGANISMO AJEDRECISTA — se hace su propio camino
 *
 *  No le damos estructura ajedrecística: ni valores de piezas, ni principios.
 *  Le damos un CUERPO en un MUNDO:
 *    · ojos   -> percibe el tablero (48 rasgos crudos)
 *    · manos  -> solo movimientos legales (validado con perft)
 *    · hambre -> ENERGÍA: capturar suma, que te capturen resta (plano)
 *    · gloria -> PLACER : ganar/dar mate/sacrificar-y-ganar suma
 *  y la misma mente del bicho original: mapa conceptual plástico, poda, ascenso
 *  a núcleo, memoria RÁPIDA (RAM) + PROFUNDA (módulos en disco), sueños, evolución.
 *
 *  Cada concepto aprende DOS valores: v_e (supervivencia) y v_p (placer). Así,
 *  una posición de sacrificio-y-mate queda con v_e bajo pero v_p alto: cuando el
 *  placer pesa, sacrifica la dama y la pérdida de energía pasa a segundo plano.
 *  Nunca le dijimos que la dama vale más: lo descubre jugando.
 *
 *  Compilar: gcc -O2 -o organismo organismo.c -lm
 *  Correr:   ./organismo --nido ./nido
 * ========================================================================== */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #define MKDIR(p) mkdir((p),0755)
#endif

/* ============================ AJEDREZ (validado con perft) ================== */
enum { EMPTY=0, P=1, N=2, B=3, R=4, Q=5, K=6 };
typedef struct { int b[64]; int side; int castle; int ep; } Pos;
typedef struct { int from,to,promo,flag; } Move;
static int file_of(int s){return s&7;} static int rank_of(int s){return s>>3;}
static int sq_of(int f,int r){return r*8+f;} static int onb(int f,int r){return f>=0&&f<8&&r>=0&&r<8;}
static void startpos(Pos* p){ memset(p->b,0,sizeof p->b); int bk[8]={R,N,B,Q,K,B,N,R};
  for(int f=0;f<8;f++){p->b[sq_of(f,0)]=bk[f];p->b[sq_of(f,1)]=P;p->b[sq_of(f,7)]=-bk[f];p->b[sq_of(f,6)]=-P;}
  p->side=1; p->castle=15; p->ep=-1; }
static int is_attacked(const Pos* p,int sq,int by){
  int f=file_of(sq),r=rank_of(sq), pr=by>0?r-1:r+1;
  for(int df=-1;df<=1;df+=2){int nf=f+df; if(onb(nf,pr)&&p->b[sq_of(nf,pr)]==by*P)return 1;}
  int kn[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
  for(int i=0;i<8;i++){int nf=f+kn[i][0],nr=r+kn[i][1]; if(onb(nf,nr)&&p->b[sq_of(nf,nr)]==by*N)return 1;}
  for(int df=-1;df<=1;df++)for(int dr=-1;dr<=1;dr++){if(!df&&!dr)continue;int nf=f+df,nr=r+dr;
    if(onb(nf,nr)&&p->b[sq_of(nf,nr)]==by*K)return 1;}
  int o[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
  for(int d=0;d<4;d++){int nf=f+o[d][0],nr=r+o[d][1];
    while(onb(nf,nr)){int pc=p->b[sq_of(nf,nr)]; if(pc){if(pc==by*R||pc==by*Q)return 1;break;} nf+=o[d][0];nr+=o[d][1];}}
  int dg[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
  for(int d=0;d<4;d++){int nf=f+dg[d][0],nr=r+dg[d][1];
    while(onb(nf,nr)){int pc=p->b[sq_of(nf,nr)]; if(pc){if(pc==by*B||pc==by*Q)return 1;break;} nf+=dg[d][0];nr+=dg[d][1];}}
  return 0;
}
static int king_sq(const Pos* p,int s){for(int i=0;i<64;i++)if(p->b[i]==s*K)return i;return -1;}
static int in_check(const Pos* p,int s){return is_attacked(p,king_sq(p,s),-s);}
static void make_move(const Pos* in,const Move* m,Pos* out){
  *out=*in; out->ep=-1; int pc=in->b[m->from]; out->b[m->from]=EMPTY; out->b[m->to]=pc;
  if(m->flag==2){int cap=m->to+(in->side>0?-8:8); out->b[cap]=EMPTY;}
  if(m->promo) out->b[m->to]=in->side*m->promo;
  if(m->flag==1) out->ep=(m->from+m->to)/2;
  if(m->flag==3){int r=rank_of(m->from); out->b[sq_of(7,r)]=EMPTY; out->b[sq_of(5,r)]=in->side*R;}
  if(m->flag==4){int r=rank_of(m->from); out->b[sq_of(0,r)]=EMPTY; out->b[sq_of(3,r)]=in->side*R;}
  int cr=out->castle;
  if(m->from==sq_of(4,0)||m->to==sq_of(4,0))cr&=~3; if(m->from==sq_of(4,7)||m->to==sq_of(4,7))cr&=~12;
  if(m->from==sq_of(7,0)||m->to==sq_of(7,0))cr&=~1; if(m->from==sq_of(0,0)||m->to==sq_of(0,0))cr&=~2;
  if(m->from==sq_of(7,7)||m->to==sq_of(7,7))cr&=~4; if(m->from==sq_of(0,7)||m->to==sq_of(0,7))cr&=~8;
  out->castle=cr; out->side=-in->side;
}
static void add(Move* L,int* n,int fr,int to,int pr,int fl){L[*n].from=fr;L[*n].to=to;L[*n].promo=pr;L[*n].flag=fl;(*n)++;}
static void addp(Move* L,int* n,int fr,int to,int fl){int r=rank_of(to);
  if(r==0||r==7){add(L,n,fr,to,Q,fl);add(L,n,fr,to,R,fl);add(L,n,fr,to,B,fl);add(L,n,fr,to,N,fl);} else add(L,n,fr,to,0,fl);}
static int gen_pseudo(const Pos* p,Move* L){ int n=0,s=p->side;
  for(int sq=0;sq<64;sq++){int pc=p->b[sq]; if(pc==0||(pc>0)!=(s>0))continue; int t=abs(pc),f=file_of(sq),r=rank_of(sq);
    if(t==P){int dr=s>0?1:-1,nr=r+dr;
      if(onb(f,nr)&&p->b[sq_of(f,nr)]==0){addp(L,&n,sq,sq_of(f,nr),0); int sr=s>0?1:6,nr2=r+2*dr;
        if(r==sr&&p->b[sq_of(f,nr2)]==0)add(L,&n,sq,sq_of(f,nr2),0,1);}
      for(int df=-1;df<=1;df+=2){int nf=f+df; if(!onb(nf,nr))continue; int to=sq_of(nf,nr),tp=p->b[to];
        if(tp!=0&&(tp>0)!=(s>0))addp(L,&n,sq,to,0); else if(to==p->ep)add(L,&n,sq,to,0,2);}}
    else if(t==N){int kn[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
      for(int i=0;i<8;i++){int nf=f+kn[i][0],nr=r+kn[i][1]; if(!onb(nf,nr))continue; int to=sq_of(nf,nr),tp=p->b[to];
        if(tp==0||(tp>0)!=(s>0))add(L,&n,sq,to,0,0);}}
    else if(t==K){for(int df=-1;df<=1;df++)for(int dr=-1;dr<=1;dr++){if(!df&&!dr)continue;int nf=f+df,nr=r+dr;
        if(!onb(nf,nr))continue;int to=sq_of(nf,nr),tp=p->b[to]; if(tp==0||(tp>0)!=(s>0))add(L,&n,sq,to,0,0);}
      int hm=s>0?0:7,kb=s>0?1:4,qb=s>0?2:8;
      if(sq==sq_of(4,hm)&&!in_check(p,s)){
        if((p->castle&kb)&&p->b[sq_of(5,hm)]==0&&p->b[sq_of(6,hm)]==0&&!is_attacked(p,sq_of(5,hm),-s)&&!is_attacked(p,sq_of(6,hm),-s))add(L,&n,sq,sq_of(6,hm),0,3);
        if((p->castle&qb)&&p->b[sq_of(3,hm)]==0&&p->b[sq_of(2,hm)]==0&&p->b[sq_of(1,hm)]==0&&!is_attacked(p,sq_of(3,hm),-s)&&!is_attacked(p,sq_of(2,hm),-s))add(L,&n,sq,sq_of(2,hm),0,4);}}
    else {int dirs[8][2],nd=0;
      if(t==B||t==Q){int d[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};for(int i=0;i<4;i++){dirs[nd][0]=d[i][0];dirs[nd][1]=d[i][1];nd++;}}
      if(t==R||t==Q){int d[4][2]={{1,0},{-1,0},{0,1},{0,-1}};for(int i=0;i<4;i++){dirs[nd][0]=d[i][0];dirs[nd][1]=d[i][1];nd++;}}
      for(int di=0;di<nd;di++){int nf=f+dirs[di][0],nr=r+dirs[di][1];
        while(onb(nf,nr)){int to=sq_of(nf,nr),tp=p->b[to];
          if(tp==0)add(L,&n,sq,to,0,0); else {if((tp>0)!=(s>0))add(L,&n,sq,to,0,0);break;} nf+=dirs[di][0];nr+=dirs[di][1];}}}
  }
  return n;
}
static int gen_moves(const Pos* p,Move* out){ Move pl[256]; int npl=gen_pseudo(p,pl),n=0;
  for(int i=0;i<npl;i++){Pos nx; make_move(p,&pl[i],&nx); if(!in_check(&nx,p->side))out[n++]=pl[i];} return n; }
/* SOLO CAPTURAS legales: para la quiescencia. Chequea is_attacked únicamente en las capturas (~5) en vez de
   en todas las jugadas (~35) — corta el grueso del costo (is_attacked era 48% del tiempo). Mismo resultado. */
static int gen_caps_legal(const Pos* p,Move* out){ Move pl[256]; int npl=gen_pseudo(p,pl),n=0;
  for(int i=0;i<npl;i++){ int cap = p->b[pl[i].to]!=0 || pl[i].flag==2 || pl[i].promo; if(!cap) continue;
    Pos nx; make_move(p,&pl[i],&nx); if(!in_check(&nx,p->side))out[n++]=pl[i]; } return n; }
static int piece_count(const Pos* p,int side){ int c=0; for(int i=0;i<64;i++){int pc=p->b[i]; if(pc&&((pc>0)==(side>0)))c++;} return c; }

/* ============================ OJOS: sensor de 48 rasgos ==================== */
#define NBASEFEAT 48           /* rasgos que le dimos nosotros */
#define KSLOT      24           /* sentidos DESPIERTOS por jugada (el cupo en vivo: acota el cómputo) */
#define GMAX       256          /* SET CALIENTE: genes cargados y activos a la vez (la "RAM" de la percepción) */
#define GPOOL      8192          /* POOL FRÍO: total de genes que el genoma puede guardar (tiende a enorme; los que no entran al caliente duermen) */
#define FEATD      72           /* 48 base + 24 ranuras despiertas */
#define GENE_OFF   45           /* en el genoma: tras los 15 instintos (15*3) empiezan los genes (= NPARAM*3) */
#define NCOND      12           /* micro-condiciones del catálogo (el SI de la regla) */
#define GENE_LEN   16           /* 6 QUÉ + 6 CUÁNDO + 4 PRIORIDAD (256 niveles -> spread rico 1-100) */
#define GENOME_CAP (GENE_OFF + GPOOL*GENE_LEN + 16)   /* buffer del genoma: holgura para todo el pool */
#define PRIO_BLEND 0.34f       /* el uso empuja la prioridad hacia su CUOTA relativa (no satura: spread rico) */
#define PRIO_BIRTH 100          /* prioridad de nacimiento alta: entra al caliente, se prueba, y el uso decide */
#ifndef QD_REAL
#ifndef EVAL_DEPTH
#define EVAL_DEPTH 3   /* profundidad de juego en las partidas que CUENTAN (sparring/vs vos). 3 = ve una jugada más que el prof de profundidad 2 (nivel 4). Subilo a 4 si querés más cálculo (más lento); bajalo a 2 si va lento. */
#endif
#define QD_REAL 3
#endif
#ifndef QD_DREAM
#define QD_DREAM 3   /* quiescencia en SUEÑOS: 1 (era 2). La quiescencia dispara is_attacked (48% del tiempo); a 1 los ticks vuelan. Las partidas que CUENTAN usan QD_REAL=4. */
#endif
static int bval(char ch){return ch=='A'?0:ch=='C'?1:ch=='G'?2:3;}
/* un gen de percepción: cuenta piezas (tipo/bando) cerca de una referencia. Emergente: puede
   evolucionar "enemigos cerca de mi rey" (peligro), "mis peones cerca del rey rival" (ataque),
   "caballos en el centro"... sin que nosotros nombremos ninguna de esas ideas. */
typedef struct { int on, type, side, ref, radius, trig, prio; } Gene;  /* prio 1-100: nace en el gen, el uso la ajusta */
static Gene GENES[GMAX]; static int NGENES=0;        /* SET CALIENTE: los genes despiertos (top por prioridad) */
static int  GHOT[GMAX];                              /* mapeo: ranura caliente i -> índice del gen en el pool del genoma */
static Gene GCOLD[GPOOL]; static int GCOLD_IX[GPOOL]; static int NGCOLD=0;  /* POOL FRÍO: genes dormidos + su índice en el genoma */
static int  GFIRE_POOL[GPOOL];                       /* usos por índice de gen en el pool (caliente o despertado): ajusta prioridad */
static int  IN_DUEL=0;                                /* 1 durante duelos: ahí NO hay despertar por contexto (genes ajenos) */
static int  NGENE_TOTAL=0;                           /* genes TOTALES en el genoma (el pool frío, puede ser enorme) */
static int  GTRACK=0;                                /* 1 solo durante el sueño real: rastrea qué genes se usan */
/* pesos de cada micro-condición: las amplias valen poco, las quirúrgicas valen más (premia precisión) */
static const int CONDW[NCOND]={ 12, 10, 10, 8, 20, 16, 12, 14, 8, 16, 24, 24 };
/* === CACHÉ DE PRESETS (memoización del selector) ===
   El ranking de sentidos depende solo de qué micro-condiciones están activas (la "firma", 12 bits).
   Cacheamos por firma el ranking ya calculado; mientras el genoma no cambie, es un lookup O(1). */
typedef struct { unsigned ver; short n; short idx[KSLOT]; } Preset;
static Preset PRESET[1<<NCOND];
static unsigned GENOME_VER=0;       /* sube cuando cambian los genes -> invalida presets (recálculo perezoso) */
static unsigned GENOME_HASH=2166136261u;  /* huella estable del genoma (para presets en disco) */
static void compute_preset(int sig){
  Preset* pr=&PRESET[sig]; int nb=0; int bi[KSLOT], bs[KSLOT];
  int hotsc[GMAX], nh=0;                              /* puntajes de los calientes (para la mediana del momento) */
  for(int gi=0; gi<NGENES; gi++){ Gene* g=&GENES[gi]; if(!g->on)continue;
    int sc=0,nc=0; for(int k=0;k<NCOND;k++) if(g->trig&(1<<k)){ nc++; if(sig&(1<<k)) sc+=CONDW[k]; }
    if(nc==0) sc=8;                  /* generalista: puntaje bajo */
    if(nh<GMAX) hotsc[nh++]=sc;
    if(nb<KSLOT){ int j=nb++; while(j>0&&bs[j-1]<sc){bs[j]=bs[j-1];bi[j]=bi[j-1];j--;} bs[j]=sc;bi[j]=gi; }
    else if(sc>bs[KSLOT-1]){ int j=KSLOT-1; while(j>0&&bs[j-1]<sc){bs[j]=bs[j-1];bi[j]=bi[j-1];j--;} bs[j]=sc;bi[j]=gi; }
  }
  /* DESPERTAR POR CONTEXTO: un gen FRÍO entra solo por esta jugada si su puntaje para este momento
     supera la MEDIANA de los calientes (más de la mitad). Juega y al siguiente preset vuelve a dormir;
     no toca su prioridad de fondo (eso lo decide el uso acumulado). Resuelve el problema del sentido
     raro-pero-clave que estaría dormido justo cuando hace falta. */
  if(!IN_DUEL && NGCOLD>0 && nh>0){
    int tmp[GMAX]; for(int i=0;i<nh;i++)tmp[i]=hotsc[i];
    for(int a=1;a<nh;a++){int v=tmp[a],b=a-1;while(b>=0&&tmp[b]>v){tmp[b+1]=tmp[b];b--;}tmp[b+1]=v;}
    int med=tmp[nh/2];                                /* mediana de los calientes activos */
    for(int ci=0; ci<NGCOLD; ci++){ Gene* g=&GCOLD[ci]; if(!g->on)continue;
      int sc=0,nc=0; for(int k=0;k<NCOND;k++) if(g->trig&(1<<k)){ nc++; if(sig&(1<<k)) sc+=CONDW[k]; }
      if(nc==0) sc=8;
      if(sc>med){                                    /* relevante AHORA -> despierta para esta jugada */
        int ref=GMAX+ci;
        if(nb<KSLOT){ int j=nb++; while(j>0&&bs[j-1]<sc){bs[j]=bs[j-1];bi[j]=bi[j-1];j--;} bs[j]=sc;bi[j]=ref; }
        else if(sc>bs[KSLOT-1]){ int j=KSLOT-1; while(j>0&&bs[j-1]<sc){bs[j]=bs[j-1];bi[j]=bi[j-1];j--;} bs[j]=sc;bi[j]=ref; } } }
  }
  /* SIEMPRE DESPIERTOS: los 5 sentidos de valoración (genes 0-4). El bicho debe VER las 5 piezas en
     CADA jugada (no solo cuando su trigger casualmente matchea), para valorar bien el material. */
  for(int gi=0; gi<5 && gi<NGENES; gi++){ if(!GENES[gi].on)continue;
    int present=0; for(int j=0;j<nb;j++) if(bi[j]==gi){present=1;break;}
    if(present)continue;
    if(nb<KSLOT){ bi[nb]=gi; bs[nb]=9999; nb++; }
    else { int worst=-1; for(int j=0;j<nb;j++){ if(bi[j]<5)continue; if(worst<0||bs[j]<bs[worst])worst=j; }
           if(worst>=0){ bi[worst]=gi; bs[worst]=9999; } } }
  pr->n=(short)nb; for(int j=0;j<nb;j++)pr->idx[j]=(short)bi[j]; pr->ver=GENOME_VER;
}
static void board_features(const Pos* p,float out[FEATD]){
  for(int i=0;i<FEATD;i++)out[i]=0;
  int cw[7]={0},cb[7]={0};
  for(int i=0;i<64;i++){int pc=p->b[i]; if(pc>0)cw[pc]++; else if(pc<0)cb[-pc]++;}
  for(int t=1;t<=6;t++){out[t-1]=cw[t]/8.0f; out[5+t]=cb[t]/8.0f;}
  out[12]=(cw[1]+cw[2]+cw[3]+cw[4]+cw[5]-cb[1]-cb[2]-cb[3]-cb[4]-cb[5])/16.0f;
  out[13]=p->side>0?1:0;
  int cen[4]={sq_of(3,3),sq_of(4,3),sq_of(3,4),sq_of(4,4)};
  for(int i=0;i<4;i++){int pc=p->b[cen[i]]; out[14+i]=pc>0?1.0f:(pc<0?-1.0f:0);}
  for(int i=0;i<4;i++)out[18+i]=(is_attacked(p,cen[i],1)?0.5f:0)+(is_attacked(p,cen[i],-1)?-0.5f:0);
  Move L[256]; int nm=gen_pseudo(p,L); out[22]=nm/40.0f; out[23]=in_check(p,p->side)?1.0f:0;
  out[24]=((p->castle&(p->side>0?1:4))?1.0f:0); out[25]=((p->castle&(p->side>0?2:8))?1.0f:0);
  for(int f=0;f<8;f++){int c=0;for(int r=0;r<8;r++)if(p->b[sq_of(f,r)]==P)c++; out[26+f]=c/2.0f;}
  for(int f=0;f<8;f++){int c=0;for(int r=0;r<8;r++)if(p->b[sq_of(f,r)]==-P)c++; out[34+f]=c/2.0f;}
  int wk=king_sq(p,1),bk=king_sq(p,-1);
  int mysh=0,opsh=0;
  if(wk>=0){int f=file_of(wk),r=rank_of(wk),sh=0;for(int df=-1;df<=1;df++)for(int dr=-1;dr<=1;dr++){int nf=f+df,nr=r+dr;if(onb(nf,nr)&&p->b[sq_of(nf,nr)]==P)sh++;}out[42]=sh/8.0f; if(p->side>0)mysh=sh; else opsh=sh;}
  if(bk>=0){int f=file_of(bk),r=rank_of(bk),sh=0;for(int df=-1;df<=1;df++)for(int dr=-1;dr<=1;dr++){int nf=f+df,nr=r+dr;if(onb(nf,nr)&&p->b[sq_of(nf,nr)]==-P)sh++;}out[43]=sh/8.0f; if(p->side>0)opsh=sh; else mysh=sh;}
  int tot=0; for(int t=1;t<=5;t++)tot+=cw[t]+cb[t]; out[44]=tot/16.0f;
  /* === EL SELECTOR DINÁMICO POR CONDICIÓN ===
     Cada gen lleva una condición (suma de micro-condiciones). En esta jugada exacta se puntúa
     cuántas matchean (POR GRADOS): más específico y mejor calzado => más puntaje. Se despiertan
     los KSLOT de mayor puntaje; el resto duerme. El puntaje cambia con cada posición. */
  int myk=p->side>0?wk:bk, opk=p->side>0?bk:wk;        /* mi rey / rey rival */
  int mybal = (int)(out[12]*16) * (p->side>0?1:-1);    /* balance desde mi lado */
  int qmine = p->side>0?cw[5]:cb[5], qopp = p->side>0?cb[5]:cw[5];
  /* dama rival cerca de mi rey / mi dama cerca del rey rival (≤3) */
  int qd_my=0,qd_op=0;
  for(int s=0;s<64;s++){ int pc=p->b[s]; if(abs(pc)!=5)continue; int opp=((pc>0)!=(p->side>0));
    int rk=opp?myk:opk; if(rk<0)continue; int dd=file_of(s)-file_of(rk),de=rank_of(s)-rank_of(rk);
    if((dd<0?-dd:dd)<=3&&(de<0?-de:de)<=3){ if(opp)qd_my=1; else qd_op=1; } }
  int cond[NCOND];
  cond[0]= out[23]>0.5f;                   /* mi rey en jaque (ya lo calculamos en out[23]) */
  cond[1]= in_check(p,-p->side);           /* rey rival en jaque */
  cond[2]= qopp>0;                         /* dama rival viva */
  cond[3]= qmine>0;                        /* mi dama viva */
  cond[4]= tot<=10;                        /* es FINAL */
  cond[5]= tot>=26;                        /* es APERTURA */
  cond[6]= mybal>=2;                       /* ventaja material */
  cond[7]= mybal<=-2;                      /* desventaja material */
  cond[8]= p->side>0;                      /* juego con blancas */
  cond[9]= mysh<2;                         /* mi rey expuesto (poco escudo) */
  cond[10]= qd_my;                         /* dama rival cerca de mi rey (PELIGRO fino) */
  cond[11]= qd_op;                         /* mi dama cerca del rey rival (ATAQUE fino) */
  /* firma = qué micro-condiciones están activas (12 bits). Todas las posiciones con la misma
     firma comparten el mismo ranking de sentidos -> lo cacheamos en un PRESET y no recalculamos. */
  int sig=0; for(int k=0;k<NCOND;k++) if(cond[k]) sig|=(1<<k);
  Preset* pr=&PRESET[sig]; if(pr->ver!=GENOME_VER) compute_preset(sig);   /* recálculo perezoso solo si cambió el genoma */
  /* computo la percepción SOLO de los despiertos del preset, en orden de puntaje */
  for(int j=0;j<pr->n;j++){ int ref=pr->idx[j]; Gene* g; int pidx;
    if(ref<GMAX){ g=&GENES[ref]; pidx=GHOT[ref]; }      /* caliente */
    else        { g=&GCOLD[ref-GMAX]; pidx=GCOLD_IX[ref-GMAX]; }   /* frío DESPERTADO por contexto */
    if(GTRACK) GFIRE_POOL[pidx]++;                      /* contá cada uso (sube su prioridad de fondo) */
    int refsq = g->ref==0 ? myk : g->ref==1 ? opk : g->ref==2 ? sq_of(3,3) : -1;
    int rf = refsq>=0?file_of(refsq):0, rr = refsq>=0?rank_of(refsq):0, cnt=0;
    for(int s=0;s<64;s++){ int pc=p->b[s]; if(!pc)continue;
      if(g->type!=0 && abs(pc)!=g->type) continue;
      int mine=((pc>0)==(p->side>0));
      if(g->side==0 && !mine) continue; if(g->side==1 && mine) continue;
      if(refsq>=0){ int df=file_of(s)-rf, dr=rank_of(s)-rr; if((df<0?-df:df)>g->radius||(dr<0?-dr:dr)>g->radius) continue; }
      cnt++; }
    out[NBASEFEAT+j] = (float)(cnt>8?8:cnt)/8.0f;
  }
}

/* ============================ MENTE: mapa conceptual ====================== */
#define MAXCONCEPTS 256
#define MAXEDGES    24
#define MODKEY_MAX  128
#define MAXLOADED   1
#define PLAN_MAX    200   /* largo máximo de un plan calculado (jugadas propias que ejecuta sin repensar) */
#define NPARAM      15
#ifndef DREAM_MAXD
#define DREAM_MAXD 3   /* profundidad de reflexión en los SUEÑOS. 1 = ticks rápidos. */
#define DREAM_PLIES 500 /* largo máx de una partida SOÑADA. Capado 200->80: lo decisivo pasa antes de la jugada 40; los finales largos casi no enseñan y matan la velocidad. El sparring (medición) sigue a 200. */
#endif
enum { P_LEARN,P_JOIN,P_DECAY,P_DEPTH,P_HUNGER,P_WE,P_WP,P_TEMP,P_MUT,P_CREAT,
       P_EAT,P_BITTEN,P_VAIN,P_METAB,P_KHUNT };   /* nuevos: gozo por comer, asco por que te coman, asco por sacrificio en vano, metabolismo (cómo el comer mueve la energía), ANSIA DE REY (deseo de cazar al rey: alto de base, crece al caer piezas enemigas) */

typedef struct {
  long gid; int module_key; float feat[FEATD];
  float v_e, v_p;
  float strength, useful; long uses, last_age;
  int core, deg, alive; int born_dream; float level; long edge_to[MAXEDGES]; float edge_w[MAXEDGES];  /* level = realidad graduada (de qué tan real fue la experiencia que lo formó) */
  int deep; int dfrom, dto; float dscore;  /* CONCEPTO PROFUNDO: rama reflexionada (jugada recomendada + consecuencia) que cuelga del tronco-situación */
  float merit; long merit_n; long merit_pos;  /* MÉRITO: calidad media en partidas REALES; merit_n = muestras; merit_pos = desenlaces positivos (eficacia = merit_pos/merit_n) */
  long pin_until;  /* PIN ESPECIAL: si >0, núcleo PROVISIONAL hasta este age — para soluciones muy eficaces de situaciones raras que nunca juntarían muestras */
} Concept;

/* Versión de los módulos de memoria en disco: depende del layout REAL del Concept.
   Si la estructura cambia (agregamos un campo), sizeof(Concept) cambia y MEM_VER también,
   así un nido viejo se RECHAZA limpio (módulo se ignora, se reconstruye) en vez de leerse
   desalineado y corromper la memoria. Antes se versionaba solo por FEATD (que no cambiaba). */
#define MEM_VER ((int)(FEATD + 1000*(int)sizeof(Concept)))

typedef struct {
  Concept mem[MAXCONCEPTS]; int nconcepts; int last_active; long age;
  double energy, placer;
  long next_gid;
  int loaded_key[MAXLOADED]; long loaded_lru[MAXLOADED]; int loaded_dirty[MAXLOADED]; int nloaded;
  long wakes, sleeps;
  char genome[GENOME_CAP]; int glen; double par[NPARAM];
  long games,wins,draws,losses,dreams,lateral,pgn_games,pgn_pos,generation,adoptions,brilliancies;
  double winrate, fitness, e_target;
  char nido[1024]; int eval_mode; int dream;
  int trial_active, trial_gene, trial_kind; long trial_until; double trial_base;  /* sentido a prueba: kind 0=crece 1=reemplaza */
  char trial_orig[GENE_LEN];   /* bloque original del sentido desafiado (para reemplazo) */
  long coevo_chal, coevo_win, coevo_veto; double coevo_last;  /* coevolución vs el campeón: desafíos, victorias, vetos al nacer, último margen */
  long spar_games, spar_wins, spar_draws, spar_losses; double spar_score, spar_elo;  /* SPARRING fijo: vara externa que NO sube con el bicho (fuerza real + Elo estimado) */
  double spar_rung_sc[3]; double spar_elo_peak;  /* puntaje POR RIVAL (prof.1/2/3) y el Elo más alto alcanzado (3 = ELO_RUNGS) */
  int school_rung; double school_score; long school_games; long observed;  /* CURRÍCULUM: nivel, puntaje vs ese nivel, partidas jugadas, partidas OBSERVADAS (mirando a la máquina) */
  /* PLAN ACTIVO: la línea que ya calculó pensando profundo. La ejecuta jugada a jugada mientras la
     realidad coincida con lo que imaginó; solo repiensa cuando el rival se sale del plan, se acaba el
     horizonte, o aparece algo claramente mejor (oportunismo). */
  Move plan[PLAN_MAX]; Pos plan_at[PLAN_MAX]; int plan_len, plan_idx, plan_side; double plan_score;
  double pval[7];   /* VALOR APRENDIDO de cada pieza (1=peón..5=dama). YA NO se infiere de parches: sale de los PESOS de V (abajo). */
  float vw[48]; double vb;  /* V(estado)=P(ganan blancas): logística sobre los 48 rasgos base, entrenada al RESULTADO real de las partidas (Montecarlo). Los pesos sobre los conteos por tipo SON los valores de pieza — emergentes, anclados a ganar/perder, no a ninguna creencia material. */
} Cell;

static char REAL_NIDO[1024];
static double rnd(void){return (double)rand()/((double)RAND_MAX+1.0);}
static int rndi(int n){return n>0?(int)(rnd()*n):0;}
static double clampd(double x,double a,double b){return x<a?a:(x>b?b:x);}
static float fdist2(const float* a,const float* b){float s=0;for(int i=0;i<FEATD;i++){float d=a[i]-b[i];s+=d*d;}return s;}
static float fdist(const float* a,const float* b){return sqrtf(fdist2(a,b));}
static int module_key(const float* x);
/* ====================== "QUÉ ESTÁ PASANDO" — clasificador de SITUACIÓN ======================
   La carpeta de cada concepto sale de la situación, no de un vector mudo: ATAQUE/DEFENSA × mi pieza ×
   pieza rival en relación. Definir qué está pasando ES la mejor forma de archivar — y de ahí emerge
   la percepción de amenazas (estar en DEFENSA de la dama = percibir el peligro). */
typedef struct { int mode; int mypiece; int enemy; } Situ;  /* mode: 0 neutral, 1 ATAQUE, 2 DEFENSA */
static int piece_val_(int t){ static const int v[7]={0,1,3,3,5,9,99}; return (t>=0&&t<7)?v[t]:0; }
/* tipo de la pieza de 'by' que ataca sq (la más barata primero = la amenaza real); 0 si ninguna */
static int attacker_type(const Pos* p,int sq,int by){
  int f=file_of(sq),r=rank_of(sq), pr=by>0?r-1:r+1;
  for(int df=-1;df<=1;df+=2){int nf=f+df; if(onb(nf,pr)&&p->b[sq_of(nf,pr)]==by*P)return P;}
  int kn[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
  for(int i=0;i<8;i++){int nf=f+kn[i][0],nr=r+kn[i][1]; if(onb(nf,nr)&&p->b[sq_of(nf,nr)]==by*N)return N;}
  int dg[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};
  for(int d=0;d<4;d++){int nf=f+dg[d][0],nr=r+dg[d][1];
    while(onb(nf,nr)){int pc=p->b[sq_of(nf,nr)]; if(pc){ if(pc==by*B)return B; if(pc==by*Q)return Q; break;} nf+=dg[d][0];nr+=dg[d][1];}}
  int o[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
  for(int d=0;d<4;d++){int nf=f+o[d][0],nr=r+o[d][1];
    while(onb(nf,nr)){int pc=p->b[sq_of(nf,nr)]; if(pc){ if(pc==by*R)return R; if(pc==by*Q)return Q; break;} nf+=o[d][0];nr+=o[d][1];}}
  for(int df=-1;df<=1;df++)for(int dr=-1;dr<=1;dr++){if(!df&&!dr)continue;int nf=f+df,nr=r+dr;
    if(onb(nf,nr)&&p->b[sq_of(nf,nr)]==by*K)return K;}
  return 0;
}
static Situ situacion(const Pos* p,int side){
  Situ s={0,0,0};
  int defv=0,defsq=-1, atkv=0,atksq=-1;
  for(int i=0;i<64;i++){ int pc=p->b[i]; if(!pc)continue; int t=pc>0?pc:-pc;
    if((pc>0)==(side>0)){ if(is_attacked(p,i,-side)&&piece_val_(t)>defv){defv=piece_val_(t);defsq=i;} }  /* mía amenazada */
    else                { if(is_attacked(p,i, side)&&piece_val_(t)>atkv){atkv=piece_val_(t);atksq=i;} }  /* rival que ataco */
  }
  if(defsq>=0 && defv>=atkv){ s.mode=2; s.mypiece=p->b[defsq]>0?p->b[defsq]:-p->b[defsq]; s.enemy=attacker_type(p,defsq,-side); }
  else if(atksq>=0){ s.mode=1; s.mypiece=attacker_type(p,atksq,side); s.enemy=p->b[atksq]>0?p->b[atksq]:-p->b[atksq]; }
  return s;
}
static int situ_key(Situ s){                                   /* 0 neutral; ATK 1..36; DEF 37..72 (< MODKEY_MAX) */
  if(s.mode==0||s.mypiece<1||s.mypiece>6||s.enemy<1||s.enemy>6) return 0;
  int base=(s.mode==1)?1:37;
  return base + (s.mypiece-1)*6 + (s.enemy-1);
}
static int situ_key_of(const Pos* p){ return situ_key(situacion(p,p->side)); }
static float CUR_REALITY=0.10f;   /* realidad graduada de la experiencia actual (autojuego bajo, maestro alto) */
static int nearest_concept(Cell* c,const Pos* p,const float* x,float* od){ int best=-1;float bd=1e18f; int key=situ_key_of(p);
  for(int i=0;i<c->nconcepts;i++){ Concept* n=&c->mem[i]; if(!n->alive||n->deep||n->module_key!=key)continue;  /* los profundos no son reconocedores simples */
    float d=fdist2(n->feat,x); if(d<bd){bd=d;best=i;}}
  if(od)*od=(best>=0)?sqrtf(bd):9.9f; return best; }

static int module_key(const float* x){
  int phase=(int)(clampd(x[44],0,0.999)*4);
  int bal  =(int)(clampd((x[12]+1)/2,0,0.999)*4);
  int side =(int)(x[13]>0.5?1:0);
  int chk  =(int)(x[23]>0.5?1:0);
  return ((phase*4+bal)*2+side)*2+chk;
}
static int find_by_gid(Cell* c,long gid){ for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive&&c->mem[i].gid==gid) return i; return -1; }
static int alloc_slot(Cell* c){
  for(int i=0;i<c->nconcepts;i++) if(!c->mem[i].alive) return i;
  if(c->nconcepts<MAXCONCEPTS) return c->nconcepts++;
  int worst=-1; float ws=1e9f;
  for(int i=0;i<c->nconcepts;i++) if(!c->mem[i].core && c->mem[i].strength<ws){ws=c->mem[i].strength;worst=i;}
  if(worst>=0) return worst;
  /* ÚLTIMO RECURSO: si TODO es núcleo y está lleno, no se traba — cede el núcleo de PEOR mérito. */
  float wm=1e9f;
  for(int i=0;i<c->nconcepts;i++){ float m=c->mem[i].merit; if(m<wm){wm=m;worst=i;} }
  return worst;
}
static void add_edge(Cell* c,int a,int b){ if(a<0||b<0||a==b)return; Concept* n=&c->mem[a]; long g=c->mem[b].gid;
  for(int e=0;e<n->deg;e++) if(n->edge_to[e]==g){ n->edge_w[e]+=0.3f; return; }
  if(n->deg<MAXEDGES){ n->edge_to[n->deg]=g; n->edge_w[n->deg]=1.0f; n->deg++; } }

static void mod_path(int key,char* out,int n){ snprintf(out,n,"%s/memoria/mod_%03d.dat",REAL_NIDO,key); }
static int loaded_idx(Cell* c,int key){ for(int i=0;i<c->nloaded;i++) if(c->loaded_key[i]==key) return i; return -1; }
static void mark_dirty(Cell* c,int key){ int i=loaded_idx(c,key); if(i>=0)c->loaded_dirty[i]=1; }
/* PURIFICACIÓN: qué concepto merece persistir. Lo demás es ruido de un solo uso y se purga al escribir
   el módulo — así el disco no se llena de miles de conceptos basura: quedan SOLO los mejores. */
static int worth_keeping(const Concept* n){
  if(n->core) return 1;                          /* núcleo (igual se guarda aparte) */
  if(n->deep) return n->uses>1 || n->strength>0.85f;  /* BIBLIOTECA DE JUGADAS: conservá toda rama que recurrió aunque sea poco; purga solo la de UN uso */
  if(n->merit_n>0) return 1;                      /* probado en partida real */
  if(n->uses>=3 || n->useful>0.5f) return 1;      /* recurrió o tuvo utilidad */
  if(n->born_dream && n->uses<=1) return 0;       /* imaginada de un solo uso = ruido */
  return n->strength>0.9f;                        /* reconocedor con algo de cuerpo */
}
static void flush_module(Cell* c,int key,int evict){
  if(c->eval_mode){ if(evict) for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive&&c->mem[i].module_key==key&&!c->mem[i].core)c->mem[i].alive=0; return; }
  char p[1200]; mod_path(key,p,sizeof p);
  /* FUSIÓN portable (sin .tmp ni rename — en Windows rename falla si el destino existe): primero LEO a memoria
     lo que ya está en disco y NO está residente (lo que no entró en RAM por falta de lugar), después sobreescribo
     el .dat directo con lo residente + lo preservado. Así un load PARCIAL no erosiona el archivo. Núcleo va aparte. */
  Concept* keep=NULL; int nkeep=0, cap=0;
  FILE* fi=fopen(p,"rb");
  if(fi){ int v2=0,c2=0; if(fread(&v2,sizeof v2,1,fi)==1 && v2==MEM_VER && fread(&c2,sizeof c2,1,fi)==1){
      for(int k=0;k<c2;k++){ Concept t; if(fread(&t,sizeof t,1,fi)!=1)break;
        if(t.module_key!=key || t.core || find_by_gid(c,t.gid)>=0 || !worth_keeping(&t)) continue;
        if(nkeep>=cap){ int nc=cap?cap*2:32; Concept* nk=realloc(keep,(size_t)nc*sizeof(Concept)); if(!nk){fclose(fi);free(keep);return;} keep=nk; cap=nc; }
        keep[nkeep++]=t; } }
    fclose(fi); }
  FILE* fo=fopen(p,"wb"); if(!fo){ free(keep); return; } int ver=MEM_VER,cnt=0;
  fwrite(&ver,sizeof ver,1,fo); long cntpos=ftell(fo); fwrite(&cnt,sizeof cnt,1,fo);
  for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive&&!c->mem[i].core&&c->mem[i].module_key==key&&worth_keeping(&c->mem[i])){
    fwrite(&c->mem[i],sizeof(Concept),1,fo); cnt++; }                       /* lo residente (estado actual) */
  for(int k=0;k<nkeep;k++){ fwrite(&keep[k],sizeof(Concept),1,fo); cnt++; } /* lo de disco que NO estaba cargado: preservado */
  fseek(fo,cntpos,SEEK_SET); fwrite(&cnt,sizeof cnt,1,fo); fclose(fo); free(keep);
  if(evict) for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive&&!c->mem[i].core&&c->mem[i].module_key==key) c->mem[i].alive=0;  /* recién ahora libero lo situacional */
}
static int load_module(Cell* c,int key){
  char p[1200]; mod_path(key,p,sizeof p); FILE* f=fopen(p,"rb"); if(!f)return 0;
  int ver=0; if(fread(&ver,sizeof ver,1,f)!=1||ver!=MEM_VER){fclose(f);return 0;}
  int cnt=0; if(fread(&cnt,sizeof cnt,1,f)!=1){fclose(f);return 0;} int loaded=0;
  for(int k=0;k<cnt;k++){ Concept tmp; if(fread(&tmp,sizeof(Concept),1,f)!=1)break;
    if(find_by_gid(c,tmp.gid)>=0) continue;   /* DEDUP: ya está residente (núcleo pineado) -> no lo dupliques */
    int s=alloc_slot(c); if(s<0)break;
    c->mem[s]=tmp; c->mem[s].alive=1; loaded++; if(tmp.gid>=c->next_gid)c->next_gid=tmp.gid+1; }
  fclose(f); return loaded;
}
static void ensure_loaded(Cell* c,int key){
  int li=loaded_idx(c,key); if(li>=0){ c->loaded_lru[li]=c->age; return; }
  if(c->nloaded>=MAXLOADED){ int lru=0; for(int i=1;i<c->nloaded;i++) if(c->loaded_lru[i]<c->loaded_lru[lru])lru=i;
    int ek=c->loaded_key[lru]; if(c->loaded_dirty[lru])flush_module(c,ek,1); else for(int i=0;i<c->nconcepts;i++)if(c->mem[i].alive&&c->mem[i].module_key==ek&&!c->mem[i].core)c->mem[i].alive=0;  /* el núcleo NO se evicta: residente */
    c->sleeps++; c->loaded_key[lru]=c->loaded_key[c->nloaded-1]; c->loaded_lru[lru]=c->loaded_lru[c->nloaded-1]; c->loaded_dirty[lru]=c->loaded_dirty[c->nloaded-1]; c->nloaded--; }
  load_module(c,key); c->wakes++;
  c->loaded_key[c->nloaded]=key; c->loaded_lru[c->nloaded]=c->age; c->loaded_dirty[c->nloaded]=0; c->nloaded++;
}
static void flush_all(Cell* c){ for(int i=0;i<c->nloaded;i++) if(c->loaded_dirty[i]){ flush_module(c,c->loaded_key[i],0); c->loaded_dirty[i]=0; } }

static void logevt(const char* tag,const char* fmt,...);   /* prototipo adelantado (lo usan reinforce y study_all_masters) */
static int perceive(Cell* c,const Pos* p,const float* x,int* created){
  int key=situ_key_of(p); ensure_loaded(c,key);
  float d; int near=nearest_concept(c,p,x,&d); if(created)*created=0;
  if(near>=0 && d<c->par[P_JOIN]){ Concept* n=&c->mem[near]; float lr=(float)c->par[P_LEARN];
    for(int i=0;i<FEATD;i++) n->feat[i]+=lr*(x[i]-n->feat[i]);
    n->strength=(float)clampd(n->strength+0.25,0,10); n->uses++; n->last_age=c->age;
    if(CUR_REALITY>n->level) n->level += 0.34f*(CUR_REALITY-n->level);   /* sube hacia experiencias más reales */
    mark_dirty(c,n->module_key);
    return near; }
  int slot=alloc_slot(c); if(slot<0) return near;
  Concept* n=&c->mem[slot]; memset(n,0,sizeof(Concept)); memcpy(n->feat,x,sizeof(float)*FEATD);
  n->gid=c->next_gid++; n->module_key=key; n->strength=0.6f; n->uses=1; n->last_age=c->age; n->alive=1;
  n->level=CUR_REALITY;                                                  /* realidad de su nacimiento */
  n->born_dream = c->dream ? 1 : 0;   /* nacida soñando (autojuego) = imaginada, en cuarentena del núcleo */
  if(created)*created=1; mark_dirty(c,key); return slot;
}
/* ===================== MÉRITO DEL NÚCLEO (idea de Nico) =====================
   El núcleo deja de ser un salón de la fama permanente: cada concepto lleva un MÉRITO
   = calidad media de los desenlaces de partidas REALES en las que disparó. La promoción
   exige haber rendido; la competencia por situación se queda con los mejores y demota a
   los inferiores. Para no zarandearse con el ruido de la media móvil (la trampa que ya nos
   mordió con el "win-rate decay"), demotar cuesta MÁS muestras y exige brecha clara. */
#define MERIT_LR      0.06f   /* qué tan rápido se mueve el mérito por partida (lento: una no lo define) */
#define MERIT_PROMO_N 8       /* muestras reales mínimas para PROMOVER por mérito */
#define MERIT_PROMO   0.05f   /* mérito mínimo (apenas positivo) para promover */
#define MERIT_MIN_N   12      /* muestras reales mínimas para DEMOTAR (cuesta más SALIR que entrar) */
#define MERIT_GAP     0.25f   /* brecha de mérito clara para que un mejor desplace a otro de la misma situación */
#define MERIT_ROT    -0.20f   /* mérito netamente negativo y muy probado = núcleo podrido (sale aunque nadie lo supere) */
#define NUC_PER_SITU  3       /* tope de núcleos por situación (top-K) */
#define MERIT_REAL    0.30f   /* CUR_REALITY mínima para que una partida cuente como rendimiento real */
#define MERIT_SPECIAL_N 3      /* apariciones mínimas para el PIN ESPECIAL (evita el azar de una sola partida) */
#define MERIT_SPECIAL  0.75f   /* eficacia (positivos/total) por encima de la cual sube PROVISIONAL al núcleo */
#define SPECIAL_WINDOW 2000    /* ticks que dura el pin especial; se refresca cada vez que vuelve a probarse eficaz */
#define NUC_CAP       (MAXCONCEPTS/2)  /* TOPE DURO: el núcleo nunca pasa de la mitad de la memoria, para que SIEMPRE quede lugar para aprender/paginar */

/* Acumula mérito de UN concepto por el desenlace de una partida real, y le da el PIN ESPECIAL si
   corresponde. outcome en [-1,1] desde la óptica del lado del concepto; w pesa por recencia/decisividad. */
static void accrue_merit(Cell* c,int idx,float outcome,float w){
  if(idx<0 || CUR_REALITY<MERIT_REAL) return;        /* índice inválido o realidad baja: no cuenta */
  Concept* n=&c->mem[idx];
  n->merit += (outcome - n->merit) * (w*MERIT_LR);
  n->merit_n++; if(outcome>0) n->merit_pos++;
  /* PIN ESPECIAL: una solución muy eficaz (>75% de positivos) para una jugada precisa que casi no se da
     —y que por eso jamás juntaría las muestras de la puerta normal— sube YA al núcleo, PROVISIONAL.
     Mientras la situación rara reaparezca y siga siendo eficaz, se refresca; si se enfría o no vuelve, vence. */
  if(n->merit_n>=MERIT_SPECIAL_N && (float)n->merit_pos/n->merit_n>MERIT_SPECIAL && n->merit>=0.5f){
    if(!n->core){ n->core=1; n->pin_until=c->age+SPECIAL_WINDOW;
      logevt("NUCLEO*","gid%ld pin especial: %ld/%ld eficaz en situación rara",n->gid,n->merit_pos,n->merit_n); }
    else if(n->pin_until>0){ n->pin_until=c->age+SPECIAL_WINDOW; }   /* provisional que sigue probándose: refresca */
  }
}

static void reinforce(Cell* c,int idx,float tgt_e,float tgt_p,float w){
  if(idx<0)return; Concept* n=&c->mem[idx]; float lr=(float)c->par[P_LEARN]*w;
  n->v_e+=lr*(tgt_e-n->v_e); n->v_p+=lr*(tgt_p-n->v_p);
  n->useful=(float)clampd(n->useful+0.05*w,0,10);
  if(CUR_REALITY>n->level) n->level += 0.34f*(CUR_REALITY-n->level);   /* realidad graduada de este refuerzo */
  mark_dirty(c,n->module_key);
  /* CONFIRMACIÓN POR REALIDAD GRADUADA: una idea soñada se gradúa si la tocó experiencia REAL —maestros o
     tu partida (c->dream==0)— O experiencia de realidad alta (escuela/observación de buen nivel, level>=0.5).
     Así el núcleo vuelve a crecer aunque saques los PGN: la observación de partidas fuertes lo alimenta. */
  if(n->born_dream && (!c->dream || n->level>=0.5f)){ n->born_dream=0; n->useful=(float)clampd(n->useful+0.3,0,10);
    logevt("IDEA-OK","la idea gid%ld se confirmó (realidad %.2f)",n->gid,n->level); }
  /* PROMOCIÓN A NÚCLEO — DOS PUERTAS:
     (1) PUERTA REAL (como la célula original): confirmada por lo real (!born_dream) + fuerza/conexiones/utilidad.
     (2) PUERTA GEMA: aun siendo del autojuego, si se probó MUCHÍSIMO con utilidad alta y sostenida,
         se gana la permanencia sola. Vara mucho más alta, para honrar los hallazgos propios sin saturar. */
  if(!n->core){
    /* MÉRITO como requisito de la puerta real: además de estar confirmado y conectado, tiene que
       haber RENDIDO en partidas reales (mérito positivo con muestra). La puerta gema —rarísima— no
       lo exige, para que el núcleo nunca se congele si la evidencia de mérito todavía es flaca. */
    int merito_ok = n->merit_n>=MERIT_PROMO_N && n->merit>MERIT_PROMO;
    /* PUERTA IMITACIÓN (maestros, PRIORIDAD ALTA): un concepto formado por ajedrez de MAESTRO (realidad
       alta, level>=0.80) que ya RECURRIÓ y está conectado entra al núcleo POR IMITACIÓN. No pide mérito
       propio: son los mejores y el bicho aprende imitando primero. Selectivo por RECURRENCIA (strength),
       no por desenlace —por eso no satura como cuando le daba crédito a cada partida ganada del maestro. */
    int puerta_imit = !n->born_dream && n->level>=0.80f && n->strength>6.0f && n->deg>=3 && n->uses>=8;
    int puerta_real = !n->born_dream && n->strength>3.5f && n->deg>=2 && n->useful>0.6f && merito_ok;
    int puerta_gema =  n->born_dream && n->strength>6.0f && n->deg>=4 && n->useful>2.5f
                       && n->uses>400 && (c->age-n->last_age)<200;
    if(puerta_imit || puerta_real || puerta_gema){ n->core=1;
      if(puerta_imit && !puerta_real) logevt("NUCLEO","imitación del maestro gid%ld al núcleo (level %.2f)",n->gid,n->level);
      if(puerta_gema) logevt("NUCLEO","gema del autojuego gid%ld ascendió (se probó sola)",n->gid); }
  }
}
/* RETENCIÓN desacoplada de la PERMANENCIA. El núcleo (impodable) decide qué es importante;
   pero un concepto NO-núcleo bien probado por el uso también merece NO olvidarse. Antes, como casi
   todo era núcleo, el disco preservaba todo de pura casualidad; al hacer el núcleo selectivo, el
   autojuego se olvidaba. Acá un concepto "establecido" (muy usado o con utilidad real) se protege
   del olvido aunque no sea núcleo; solo muere el débil, poco usado y viejo (el ruido de un solo uso). */
#define RETAIN_USES   20      /* usado esto o más => retenido (patrón recurrente, no ruido) */
#define RETAIN_USEFUL 1.0f    /* o con esta utilidad acumulada => retenido */
/* COMPETENCIA DEL NÚCLEO: el núcleo se mantiene como el "mejor-de" vivo, no como un salón de la fama
   permanente. (a) Poda lo PODRIDO: un núcleo con mérito netamente negativo y MUY probado en reales sale.
   (b) Por cada situación, si hay más de NUC_PER_SITU núcleos, demota a los de peor mérito —pero solo si
   tienen muestra suficiente y la brecha con el que los supera es clara (anti-ruido de media móvil).
   Demotar NO borra: vuelve a concepto normal y puede reconquistar el núcleo si mejora. */
/* PRIORIDAD del núcleo: el maestro pesa MÁS que el autojuego. La columna es `level` (imitación: maestro
   ≈0.95, escuela 0.3-0.9, sueño 0.1) y el mérito propio suma como bono menor. Así un patrón de maestro
   (level alto) gana el lugar aunque no tenga mérito propio, y un concepto de autojuego necesita rendir
   mucho para superarlo. Pedido de Nico: aprende imitando primero, el PGN nutre el núcleo con prioridad. */
/* Saca un concepto del núcleo y lo PERSISTE bien: si su módulo está cargado, vuelve a concepto normal
   (se escribirá no-núcleo); si no está cargado, es huérfano (no quedaría guardado en ningún lado) y se suelta. */
static void demote_core(Cell* c, Concept* n){
  n->core=0; n->pin_until=0;
  if(loaded_idx(c,n->module_key)>=0) mark_dirty(c,n->module_key);
  else n->alive=0;
}
static float nucleo_prio(const Concept* n){ return n->level + 0.5f*n->merit; }

static void compete_nucleo(Cell* c){
  for(int i=0;i<c->nconcepts;i++){ Concept* n=&c->mem[i];
    if(!n->alive||!n->core||n->deep) continue;
    /* (a) PIN ESPECIAL provisional: solo lo rige su vencimiento. Si no se refrescó (la situación rara
       no volvió o dejó de ser eficaz), cae del núcleo y vuelve a concepto normal. */
    if(n->pin_until>0){ if(c->age>n->pin_until){ demote_core(c,n);
        logevt("DEMOTE*","gid%ld vence el pin especial (no recurrió/eficaz)",n->gid); } continue; }
    /* (b) PODRIDO: núcleo permanente con mérito netamente negativo y muy probado -> sale. */
    if(n->merit_n>=2*MERIT_MIN_N && n->merit<MERIT_ROT){ demote_core(c,n);
      logevt("DEMOTE","gid%ld sale del núcleo: podrido (mérito %.2f en %ld reales)",n->gid,n->merit,n->merit_n); } }
  /* (c) COMPETENCIA por situación: solo entre núcleo PERMANENTE (los provisionales no compiten acá). */
  for(int k=0;k<MODKEY_MAX;k++){
    int idx[MAXCONCEPTS], nn=0;
    for(int i=0;i<c->nconcepts;i++){ Concept* n=&c->mem[i];
      if(n->alive&&n->core&&!n->deep&&n->pin_until==0&&n->module_key==k) idx[nn++]=i; }
    if(nn<=NUC_PER_SITU) continue;
    for(int a=0;a<nn;a++) for(int b=a+1;b<nn;b++)               /* orden por PRIORIDAD desc (el maestro pesa más) */
      if(nucleo_prio(&c->mem[idx[b]])>nucleo_prio(&c->mem[idx[a]])){ int t=idx[a];idx[a]=idx[b];idx[b]=t; }
    float topp=nucleo_prio(&c->mem[idx[NUC_PER_SITU-1]]);       /* prioridad del peor que SE QUEDA */
    for(int r=NUC_PER_SITU;r<nn;r++){ Concept* n=&c->mem[idx[r]];
      if(n->merit_n>=MERIT_MIN_N && (topp-nucleo_prio(n))>=MERIT_GAP){ float pr=nucleo_prio(n); demote_core(c,n);   /* solo demota autojuego con muestra; el maestro (merit_n=0) es inmune acá */
        logevt("DEMOTE","gid%ld cede el núcleo de situ %d (prio %.2f, lo supera %.2f)",n->gid,k,pr,topp); } }
  }
  /* (d) TOPE DURO: el núcleo JAMÁS se come la memoria. Si excede NUC_CAP, demota los de PEOR PRIORIDAD
     (level+mérito) hasta volver al tope. Como el maestro tiene level alto, lo último que se va es la
     imitación: el núcleo queda dominado por ajedrez de maestro, con el autojuego ganándose su lugar si rinde. */
  int ncore=0; for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive&&c->mem[i].core&&!c->mem[i].deep) ncore++;
  while(ncore>NUC_CAP){
    int worst=-1; float wm=1e9f;
    for(int i=0;i<c->nconcepts;i++){ Concept* n=&c->mem[i]; if(!n->alive||!n->core||n->deep)continue;
      if(nucleo_prio(n)<wm){wm=nucleo_prio(n);worst=i;} }
    if(worst<0) break;
    demote_core(c,&c->mem[worst]); ncore--; }
}

static void prune_memory(Cell* c){ double dec=c->par[P_DECAY];
  for(int i=0;i<c->nconcepts;i++){ Concept* n=&c->mem[i]; if(!n->alive||n->core)continue;
    int establecido = n->uses>=RETAIN_USES || n->useful>RETAIN_USEFUL;   /* bien probado por el uso */
    n->strength*=(float)dec;
    if(!establecido && n->strength<0.12f && c->age-n->last_age>40) n->alive=0; } }

/* ============================ INTUICIÓN + CÁLCULO ========================= */
/* V(estado) = P(ganan blancas), logística sobre los 48 rasgos BASE (estables). z = vb + Σ vw·x. */
static double vpredict(Cell* c,const float* x){
  double z=c->vb; for(int j=0;j<48;j++) z+=(double)c->vw[j]*x[j];
  if(z>30)z=30; else if(z<-30)z=-30; return 1.0/(1.0+exp(-z));
}
/* Los VALORES de pieza = cuánto sube V por tener una de ese tipo (blancas) menos cuánto la baja tenerla el
   rival. Eso es exactamente el peso que V le da al material. Normalizo por el mayor y suavizo. */
static void read_piece_values(Cell* c){
  double raw[7]={0}; for(int t=1;t<=5;t++) raw[t]=(double)c->vw[t-1]-(double)c->vw[5+t];
  double mx=1e-6; for(int t=1;t<=5;t++) if(raw[t]>mx)mx=raw[t];
  for(int t=1;t<=5;t++){ double nv=raw[t]/mx; if(nv<0)nv=0; if(nv>1)nv=1;
    c->pval[t]=0.92*c->pval[t]+0.08*nv; }   /* media móvil: estable mientras V calibra */
}
static double intuition(Cell* c,const Pos* p){
  float x[FEATD]; board_features(p,x); float d; int nr=nearest_concept(c,p,x,&d);
  double conv=0;
  if(nr>=0 && d<=0.9f){ double conf=1.0/(1.0+3.0*d);
    double mood=c->placer/25.0;                 /* 0..1: qué tan contento/ganando está */
    double we=c->par[P_WE];
    double wp=c->par[P_WP]*(mood>0.5 ? 1.0+0.6*(mood-0.5)*2.0 : 2.5);
    /* CONVERTIR (corrige "solo empata"): el lazo viejo bajaba wp cuando iba ganando -> apagaba el canal de
       ANSIA DE REY justo cuando hay que rematar -> material plano sin rumbo al mate -> barajaba -> tablas.
       Ahora al revés: ABAJO/parejo (mood<=0.5) wp neutro ~1.0 -> AGUANTA sin regalar (mantiene pocas
       derrotas). ARRIBA (mood 0.5..1) wp sube 1.0->1.6x -> PRESIONA por el mate, convierte la ventaja en
       victoria. No colgar piezas ya lo cubren el material (mb) y la quiescence; bajar wp solo lo volvía
       pasivo. Así el king_drive por fin "muerde": más ansia -> más mood -> MÁS presión (lazo positivo). */
    conv=(we*c->mem[nr].v_e + wp*c->mem[nr].v_p)*conf; }
  /* MATERIAL con los valores APRENDIDOS, reusando los conteos por tipo que board_features YA calculó
     (x[0..5]=blancas/8, x[6..11]=negras/8): sin re-barrer 64 casillas (intuition es hot). Comprimido a
     rango sano (peón ~0.3, pieza ~1 => pieza/peón ~3x). */
  double mb=0; for(int t=1;t<=5;t++){ double cnt=((double)x[t-1]-(double)x[5+t])*8.0; mb += (0.1+0.9*c->pval[t])*cnt; }  /* valores de V; piso chico al peón para que no quede en cero */
  mb = (p->side>0? mb : -mb);
  return conv + mb;
}
/* búsqueda de quietud: extiende SOLO por capturas hasta que la posición se aquieta,
   y recién ahí juzga con su intuición. Mata el efecto horizonte (colgarse piezas)
   sin darle valores de pieza: simplemente juega el lío de capturas hasta el final. */
static double quiesce(Cell* c,const Pos* p,double a,double b,int qd){
  int chk=in_check(p,p->side);
  if(!chk){ double stand=intuition(c,p); if(stand>=b)return b; if(stand>a)a=stand; }
  if(qd<=0) return chk? intuition(c,p) : a;
  Move L[256]; int n = chk? gen_moves(p,L) : gen_caps_legal(p,L);   /* en jaque: todas las evasiones; tranquilo: SOLO capturas (mucho menos is_attacked) */
  if(n==0) return chk? -100.0 : a;
  for(int i=0;i<n;i++){
    Pos nx; make_move(p,&L[i],&nx);
    double sc=-quiesce(c,&nx,-b,-a,qd-1);
    if(sc>a){ a=sc; if(a>=b) return b; }
  }
  return a;
}
/* ordena capturas primero: mejora la poda alfa-beta sin usar valores de pieza */
static void order_caps_first(const Pos* p,Move* L,int n){
  int j=0; for(int i=0;i<n;i++) if(p->b[L[i].to]!=0||L[i].flag==2||L[i].promo){ Move t=L[j];L[j]=L[i];L[i]=t; j++; }
}
static double search(Cell* c,const Pos* p,int depth,double a,double b){
  Move L[256]; int n=gen_moves(p,L);
  if(n==0) return in_check(p,p->side)? -100.0 : 0.0;
  if(depth==0) return quiesce(c,p,a,b,c->dream?QD_DREAM:QD_REAL);
  order_caps_first(p,L,n);
  double best=-1e9;
  for(int i=0;i<n;i++){Pos nx;make_move(p,&L[i],&nx); double sc=-search(c,&nx,depth-1,-b,-a);
    if(sc>best)best=sc; if(best>a)a=best; if(a>=b)break;}
  return best;
}
/* ===================== REFLEXIÓN PROFUNDA (el concepto que PIENSA) =====================
   En situación caliente (ataque/defensa en juego) el bicho arma el tablero imaginario y reflexiona:
   mi jugada -> qué me hacen -> cómo respondo, profundizando 1..7 con PODA (descarta lo que nadie jugaría),
   cortando temprano si la consecuencia es buena y el peligro se fue. Cosecha la mejor rama —o TODAS las
   casi-empatadas— como conceptos PROFUNDOS que cuelgan del tronco-situación. La próxima vez RECONOCE la
   rama (cache de pensamiento: lento al principio, rápido después) y la SIENTE como valor (camino 2). */
static int spar_eval(const Pos* p);
static int spar_quiesce(const Pos* p,int alpha,int beta,int qd);
static int plausible_move(const Pos* p,const Move* m){
  if(p->b[m->to]!=0 || m->promo || m->flag==2) return 1;            /* captura / promoción / al paso */
  if(is_attacked(p,m->from,-p->side)) return 1;                      /* saca una pieza amenazada */
  Pos nx; make_move(p,m,&nx); if(in_check(&nx,nx.side)) return 1;    /* da jaque */
  int ek=king_sq(p,-p->side);
  if(ek>=0){ int d0=abs(file_of(m->from)-file_of(ek))+abs(rank_of(m->from)-rank_of(ek));
             int d1=abs(file_of(m->to)-file_of(ek))+abs(rank_of(m->to)-rank_of(ek));
             if(d1<d0) return 1; }                                   /* avanza hacia el rey rival */
  return 0;                                                          /* lo demás: nadie lo recorre tácticamente */
}
static int reflect_ng(const Pos* p,int depth,int alpha,int beta){
  if(depth<=0) return spar_quiesce(p,alpha,beta,4);
  Move L[256]; int n=gen_moves(p,L);
  if(n==0) return in_check(p,p->side)? -29000-depth : 0;
  int explored=0,best=-100000;
  for(int i=0;i<n;i++){ if(!plausible_move(p,&L[i]))continue;        /* PODA: solo lo que alguien jugaría */
    explored++; Pos nx; make_move(p,&L[i],&nx);
    int sc=-reflect_ng(&nx,depth-1,-beta,-alpha);
    if(sc>best)best=sc; if(best>alpha)alpha=best; if(alpha>=beta)break; }
  if(!explored) return spar_quiesce(p,alpha,beta,4);
  return best;
}
/* consecuencia de un movimiento: profundiza 1..maxd hasta que sea clara */
static int reflect_move(const Pos* p,const Move* m,int maxd){
  Pos nx; make_move(p,m,&nx); int side=p->side; int sc=0;
  for(int d=1; d<=maxd; d++){
    sc=-reflect_ng(&nx,d-1,-100000,100000);
    int ks=king_sq(&nx,side); int safe=(ks<0)||!is_attacked(&nx,ks,-side);
    if(safe && sc>=100) break;                  /* buena y el peligro se fue -> corta (profundidad baja) */
    if(d>=2 && (sc>=300||sc<=-300)) break;       /* decisivo -> no hace falta más */
  }
  return sc;
}
static void harvest_deep(Cell* c,int skey,const float* x,const Move* m,int score){
  for(int j=0;j<c->nconcepts;j++){ Concept* d=&c->mem[j];
    if(d->alive&&d->deep&&d->module_key==skey&&d->dfrom==m->from&&d->dto==m->to){   /* ya existe: refuerza */
      d->dscore=0.7f*d->dscore+0.3f*score; d->strength=(float)clampd(d->strength+0.3,0,10);
      d->uses++; d->useful=(float)clampd(d->useful+0.05,0,10); d->last_age=c->age;
      if(CUR_REALITY>d->level)d->level+=0.34f*(CUR_REALITY-d->level); mark_dirty(c,skey); return; } }
  int slot=alloc_slot(c); if(slot<0)return; Concept* d=&c->mem[slot]; memset(d,0,sizeof(Concept));
  memcpy(d->feat,x,sizeof(float)*FEATD); d->gid=c->next_gid++; d->module_key=skey;
  d->deep=1; d->dfrom=m->from; d->dto=m->to; d->dscore=(float)score; d->strength=0.8f; d->uses=1;
  d->last_age=c->age; d->alive=1; d->level=CUR_REALITY; d->born_dream=c->dream?1:0; mark_dirty(c,skey);
}
/* sesga la elección con la reflexión y cosecha ramas; bonus[i] se suma al puntaje del movimiento i */
static long DEEP_HARVEST=0, DEEP_HITS=0;
static void deep_pass(Cell* c,const Pos* p,Move* L,int n,double* bonus){
  for(int i=0;i<n;i++)bonus[i]=0;
  Situ su=situacion(p,p->side); if(su.mode==0) return;              /* solo situaciones calientes */
  int skey=situ_key(su); ensure_loaded(c,skey);
  float x[FEATD]; board_features(p,x);
  int hit=-1; float hd=1e9f;                                        /* ¿rama profunda cacheada que matchee? */
  for(int j=0;j<c->nconcepts;j++){ Concept* d=&c->mem[j]; if(!d->alive||!d->deep||d->module_key!=skey)continue;
    float dd=fdist2(d->feat,x); if(dd<hd){hd=dd;hit=j;} }
  if(hit>=0 && sqrtf(hd)<(float)c->par[P_JOIN]*1.5f){               /* RECONOCIDA: usa la intuición cacheada */
    Concept* d=&c->mem[hit]; double w=0.5+0.5*d->level;            /* CAMINO 2: la SIENTE como valor, pesada por nivel */
    for(int i=0;i<n;i++) if(L[i].from==d->dfrom&&L[i].to==d->dto) bonus[i]+=d->dscore*0.01*w;
    d->uses++; d->last_age=c->age; DEEP_HITS++; return;
  }
  int maxd = c->dream ? DREAM_MAXD : (c->eval_mode?3:7);   /* SUEÑOS: prof 1 (ticks rápidos); sparring 3 / vivo 7 */
  int rsc[256]; int bestsc=-1000000,any=0;
  for(int i=0;i<n;i++){ if(!plausible_move(p,&L[i])){rsc[i]=-1000000;continue;}
    rsc[i]=reflect_move(p,&L[i],maxd); any=1; if(rsc[i]>bestsc)bestsc=rsc[i]; }
  if(!any) return;
  int margin=40;                                                   /* poquita diferencia: cosecha casi-empatadas */
  for(int i=0;i<n;i++){ if(rsc[i]<=-1000000)continue;
    bonus[i]+=rsc[i]*0.01;
    if(rsc[i]>=bestsc-margin){ harvest_deep(c,skey,x,&L[i],rsc[i]); DEEP_HARVEST++; } }
}
static int pos_eq(const Pos* a,const Pos* b){
  if(a->side!=b->side||a->castle!=b->castle||a->ep!=b->ep) return 0;
  for(int i=0;i<64;i++) if(a->b[i]!=b->b[i]) return 0;
  return 1;
}
/* reflexión que guarda la LÍNEA principal (PV) en pv[], no solo el puntaje */
static int reflect_pv(const Pos* p,int depth,int alpha,int beta,Move* pv,int* pvlen){
  *pvlen=0;
  if(depth<=0) return spar_quiesce(p,alpha,beta,4);
  Move L[256]; int n=gen_moves(p,L);
  if(n==0) return in_check(p,p->side)? -29000-depth : 0;
  int explored=0,best=-100000; Move bestmv={-1,-1,0,0};
  Move childpv[24]; int childlen=0; Move bestchild[24]; int bestclen=0;
  for(int i=0;i<n;i++){ if(!plausible_move(p,&L[i]))continue;
    explored++; Pos nx; make_move(p,&L[i],&nx);
    int sc=-reflect_pv(&nx,depth-1,-beta,-alpha,childpv,&childlen);
    if(sc>best){ best=sc; bestmv=L[i]; bestclen=childlen<22?childlen:22;
      for(int k=0;k<bestclen;k++)bestchild[k]=childpv[k]; }
    if(best>alpha)alpha=best; if(alpha>=beta)break; }
  if(!explored) return spar_quiesce(p,alpha,beta,4);
  pv[0]=bestmv; int m=1; for(int k=0;k<bestclen && m<22;k++)pv[m++]=bestchild[k]; *pvlen=m;
  return best;
}
/* arma el PLAN arrancando con la jugada ya elegida 'first', y sigue con la mejor línea desde ahí.
   Guarda mis jugadas + la posición exacta que espero enfrentar antes de cada una. */
static void build_plan_from(Cell* c,const Pos* p,const Move* first,int maxd){
  c->plan_len=0; c->plan_idx=0; c->plan_side=p->side;
  c->plan_at[0]=*p; c->plan[0]=*first; int mine=1;       /* paso 0: mi jugada elegida, posición actual */
  Pos cur; make_move(p,first,&cur);                       /* ahora responde el rival */
  Move pv[24]; int pvlen=0; reflect_pv(&cur,maxd,-100000,100000,pv,&pvlen);
  for(int k=0;k<pvlen && mine<PLAN_MAX;k++){              /* pv: rival(0), mía(1), rival(2), mía(3)... */
    if((k%2)==1){ c->plan_at[mine]=cur; c->plan[mine]=pv[k]; mine++; }  /* mis jugadas en índices impares */
    Pos nx; make_move(&cur,&pv[k],&nx); cur=nx;
  }
  c->plan_len=mine; c->plan_idx=1;   /* el paso 0 es la jugada que devolvemos YA; la próxima vez miro plan[1] */
}
static long PLAN_FOLLOW=0, PLAN_BREAK=0;
/* ¿hay un plan vigente para ESTA posición real? Devuelve 1 y la jugada si lo sigue; 0 si hay que repensar.
   Sigue el plan mientras la realidad coincida con lo imaginado; lo abandona si surge algo claramente mejor. */
static int try_plan(Cell* c,const Pos* p,Move* out){
  if(c->plan_len<=0||c->plan_idx>=c->plan_len) return 0;           /* sin plan o horizonte agotado -> pensar */
  if(c->plan_side!=p->side) return 0;
  if(!pos_eq(p,&c->plan_at[c->plan_idx])) return 0;                /* el rival se salió del plan -> repensar */
  Move planned=c->plan[c->plan_idx];
  Move L[256]; int n=gen_moves(p,L); int ok=0;
  for(int i=0;i<n;i++) if(L[i].from==planned.from&&L[i].to==planned.to&&L[i].promo==planned.promo){ok=1;break;}
  if(!ok) return 0;
  int planned_sc=reflect_move(p,&planned,2), bestother=-1000000;   /* OPORTUNISMO: vistazo barato (prof. 2) */
  for(int i=0;i<n;i++){ if(L[i].from==planned.from&&L[i].to==planned.to)continue;
    if(!plausible_move(p,&L[i]))continue;
    int sc=reflect_move(p,&L[i],2); if(sc>bestother)bestother=sc; }
  if(bestother>=planned_sc+200){ PLAN_BREAK++; return 0; }         /* algo claramente mejor surgió -> abandona el plan */
  *out=planned; c->plan_idx++; PLAN_FOLLOW++; return 1;            /* sigue jugando lo que ya pensó */
}
static Move choose(Cell* c,const Pos* p,double extra_temp,int* terminal){
  Move L[256]; int n=gen_moves(p,L);
  if(n==0){ if(terminal)*terminal=1; Move z={-1,-1,0,0}; return z; }
  if(terminal)*terminal=0;
  Move planmv; if(try_plan(c,p,&planmv)) return planmv;            /* PLAN ACTIVO: ejecuta sin repensar */
  int depth=1;   /* base RÁPIDA en medio juego parejo */
  { int mine=piece_count(p,p->side), opp=piece_count(p,-p->side);
    if(mine-opp>=1 || mine+opp<=12) depth=(int)clampd(c->par[P_DEPTH],2,3); }   /* PROFUNDIZA donde se CONVIERTE: con ventaja material o en final (pocas piezas, rama barata). Ver 2 jugadas permite rematar/coronar en vez de quedarse en tablas. Respeta la profundidad que el bicho EVOLUCIONÓ (~2) */
  double hunger=c->par[P_HUNGER];
  double sc[256]; double best=-1e9; int bi=0; int before=piece_count(p,-p->side);
  double dbonus[256]; deep_pass(c,p,L,n,dbonus);    /* REFLEXIÓN PROFUNDA: sesga y cosecha ramas (vacío si la posición es tranquila) */
  int wander = c->dream && c->par[P_CREAT]>0.01 && rnd()<0.6;   /* curiosidad intermitente (y más barata) */
  for(int i=0;i<n;i++){ Pos nx; make_move(p,&L[i],&nx);
    double s=-search(c,&nx,depth-1,-1e9,1e9);
    int after=piece_count(&nx,-p->side); if(after<before) s+=hunger*1.0;
    s += dbonus[i];                                 /* la consecuencia reflexionada / la rama reconocida */
    if(wander){                                      /* CURIOSIDAD: tira hacia posiciones nunca vistas */
      float xf[FEATD]; board_features(&nx,xf); float d; nearest_concept(c,&nx,xf,&d);
      double nov = d>2.0?2.0:d; s += c->par[P_CREAT]*0.6*nov; }
    sc[i]=s; if(s>best){best=s;bi=i;} }
  double temp = c->eval_mode ? 0.0 : (c->par[P_TEMP]+extra_temp);   /* en lo que SE MIDE/CUENTA (sparring, tus partidas) juega su MEJOR jugada; la exploración queda para el aprendizaje */
  int pick=bi;
  if(temp>0.02){
    double sum=0,pr[256]; for(int i=0;i<n;i++){pr[i]=exp((sc[i]-best)/(temp*1.5)); sum+=pr[i];}
    double r=rnd()*sum,acc=0; for(int i=0;i<n;i++){acc+=pr[i]; if(r<=acc){pick=i;break;}}
    if(pick!=bi)c->lateral++;
  }
  /* tras elegir, si la situación es caliente, GUARDA el plan (la línea) arrancando por la jugada elegida */
  if(situacion(p,p->side).mode!=0){ int maxd=c->dream?DREAM_MAXD:(c->eval_mode?3:7); build_plan_from(c,p,&L[pick],maxd); }
  else c->plan_len=0;
  return L[pick];
}

/* ============================ JUGAR Y APRENDER ============================ */
static Move spar_best(const Pos* p,int depth);          /* rival de la escalera (definido más abajo) */
static Move ladder_move(const Pos* p,int rung);
static void logevt(const char* tag,const char* fmt,...);
/* opp_rung<0 => AUTOJUEGO (sueño contra sí mismo, modo viejo, reversible).
   opp_rung>=0, observe=0 => CURRÍCULUM: el bicho juega 'bicho_side' y aprende; el otro lado lo juega la escalera.
   observe=1 => OBSERVADOR: máquina vs máquina (ambos lados la escalera, un nivel arriba); el bicho NO mueve,
                solo percibe AMBOS lados y refuerza ideas con el desenlace (aprende mirando, como un aprendiz). */
static int play_and_learn_ex(Cell* c,int learning,int opp_rung,int bicho_side,int observe){
  Pos p; startpos(&p); c->dream=1; c->plan_len=0;   /* plan fresco cada partida */
  int pidx[400], pside[400]; int hn=0;
  static float vbuf[128][48]; int vn=0;   /* posiciones vividas (rasgos base) para entrenar V al final */
  int cap_w=0,cap_b=0, min_pieces_w=16,min_pieces_b=16; int chk_tot=0,katk_tot=0;
  double temp_dream = (learning && opp_rung<0) ? c->par[P_CREAT]*0.3 : 0;   /* ESCUELA (opp_rung>=0): temperatura 0 -> juega SU MEJOR jugada en las partidas que lo califican (convierte ganadas en victorias, no en tablas por ruido). La curiosidad la aporta el wander, no el ruido de temperatura. Sólo el autojuego viejo (opp_rung<0) conserva algo de exploración */
  int result=0, ply, mated=0;
  for(ply=0; ply<DREAM_PLIES; ply++){
    int term=0; int before_enemy=piece_count(&p,-p.side);
    int my_turn = !observe && ((opp_rung<0) || (p.side==bicho_side));  /* en observación NO da órdenes */
    int watch = observe || my_turn;                                    /* percibe: observando, AMBOS lados */
    if(watch && !c->eval_mode){ float x[FEATD]; board_features(&p,x); int cr; int idx=perceive(c,&p,x,&cr);
      if(hn<400){pidx[hn]=idx; pside[hn]=p.side; hn++;}
      if(vn<128){ for(int j=0;j<48;j++) vbuf[vn][j]=x[j]; vn++; }   /* guardo los rasgos base para V */
      if(c->last_active>=0) add_edge(c,c->last_active,idx); c->last_active=idx; }
    Move m;
    if(my_turn) m=choose(c,&p,temp_dream,&term);
    else { Move L[256]; int nL=gen_moves(&p,L); if(nL==0)term=1; else m=ladder_move(&p,opp_rung); }
    if(term){ if(in_check(&p,p.side)){ result=-p.side; mated=1; } else result=0; break; }
    Pos nx; make_move(&p,&m,&nx);
    int after_enemy=piece_count(&nx,-p.side);
    if(after_enemy<before_enemy){ if(p.side>0)cap_w++; else cap_b++; }
    if(in_check(&nx,nx.side)) chk_tot++;                    /* el que movió dio JAQUE */
    {int ek=king_sq(&nx,nx.side); if(ek>=0){ int df=file_of(m.to)-file_of(ek),dr=rank_of(m.to)-rank_of(ek);
       if((df<0?-df:df)<=2 && (dr<0?-dr:dr)<=2) katk_tot++; }}   /* aterrizó cerca del REY rival */
    p=nx;
    int pw=piece_count(&p,1),pb=piece_count(&p,-1); if(pw<min_pieces_w)min_pieces_w=pw; if(pb<min_pieces_b)min_pieces_b=pb;
    if(ply==199){ int dpw=piece_count(&p,1)-piece_count(&p,-1); result = dpw>1?1:(dpw<-1?-1:0); }
  }
  int pw_end=piece_count(&p,1), pb_end=piece_count(&p,-1);
  double econ_w=tanh((cap_w-(16-min_pieces_w))/3.0);
  double econ_b=tanh((cap_b-(16-min_pieces_b))/3.0);
  /* sacrificio de verdad = GANAR con menos (o igual) material que el rival */
  int win_side = result>0?1:(result<0?-1:0);
  int win_pieces = result>0?pw_end:pb_end, lose_pieces = result>0?pb_end:pw_end;
  int deficit = lose_pieces - win_pieces;          /* >0 => el que ganó tenía menos piezas */
  int brilliant = (win_side!=0) && mated && deficit>=2;   /* mató estando en desventaja material */
  if(learning && !c->eval_mode){
    for(int i=0;i<hn;i++){ int S=pside[i];
      double my_caps = S>0?cap_w:cap_b;            /* piezas que comió ESTE lado */
      double my_loss = S>0?cap_b:cap_w;            /* piezas que le comieron a ESTE lado */
      int won_s=(result*S>0), lost_s=(result*S<0);
      /* ENERGÍA aprendida (v_e): sube al comer, baja al perder piezas, ~0 si nadie se come (econ ya lo hace) */
      float te=(float)(S>0?econ_w:econ_b);
      /* PLACER aprendido (v_p) — los PESOS ahora vienen del GENOMA: cada individuo evoluciona su
         propio carácter afectivo (glotón / romántico / cauto), en vez de un valor fijo a dedo. */
      int    opp_end = S>0?pb_end:pw_end;                  /* piezas ENEMIGAS al final */
      int    my_end  = S>0?pw_end:pb_end;                  /* MIS piezas al final */
      double enemy_stripped = (16 - opp_end)/16.0;         /* 0=enemigo entero .. ~1=desnudo */
      double own_stripped   = (16 - my_end )/16.0;         /* cuánto me desnudaron a MÍ */
      double kpress = tanh((chk_tot+katk_tot)/12.0);       /* presión REAL sobre el rey en la partida */
      double eat_joy = tanh(my_caps/4.0)*c->par[P_EAT];               /* gozo por COMER (gen P_EAT) */
      double eaten   = -tanh(my_loss/4.0)*c->par[P_BITTEN]*(1.0+own_stripped); /* ASCO por que te coman (gen P_BITTEN): CRECE cuanto más te desnudan -> conservador */
      double win_joy = won_s*(0.30 + (brilliant?0.70:0.0));           /* gloria: ganar / mate / sacrificio que pagó (fijo) */
      double vain    = (lost_s && (my_loss-my_caps)>=2)? -c->par[P_VAIN]:0.0;  /* asco por sacrificio EN VANO (gen P_VAIN) */
      double king_joy= won_s ? c->par[P_KHUNT]*(0.5+enemy_stripped)*kpress : 0.0;   /* ANSIA DE REY (gen P_KHUNT): alta de base, MÁS con cada pieza enemiga caída — pero SÓLO premia si GANÓ. Atacar y MORIR no da placer: aprende a cazar al rey para dar MATE, no a tirarse en vano */
      float tp=(float)clampd(eat_joy+eaten+win_joy+vain+king_joy, -1.0, 1.0);
      float recency = 0.5f + 0.5f*((float)i/(hn>1?hn-1:1));           /* lo cercano al desenlace pesa más */
      reinforce(c,pidx[i],te,tp,recency);
      accrue_merit(c, pidx[i], won_s?1.0f:(lost_s?-1.0f:0.0f), recency); }  /* MÉRITO + pin especial: rendimiento real de cada concepto que disparó */
    /* PULSOS COMO ELECTROCARDIOGRAMA: cada partida los sacude — TAMBIÉN observando (calibra energía/placer mirando) */
    double feed = tanh((cap_w+cap_b)/8.0);                 /* actividad de captura */
    double swing = econ_w - econ_b;                        /* flujo de material SIGNADO */
    c->energy += (c->e_target - c->energy)*0.12 + 1.9*swing + c->par[P_METAB]*(feed-0.5);  /* metabolismo (gen): cuánto el comer mueve la energía */
    c->energy = clampd(c->energy,0,25);
    double aggro = tanh((chk_tot+katk_tot)/12.0);          /* qué tan violenta fue la partida */
    /* PLACER COMO EL HAMBRE (pedido de Nico): tiene un OBJETIVO según cómo le fue al bicho —ganó/perdió,
       material a favor/en contra, presión al rey, brillo— y se RELAJA hacia él pulsando. Sube hacia ~25
       cuando va ganando/atacando, baja hacia ~10 cuando lo muelen. Se calibra solo, ya no se clava en 9. */
    int bside = observe? 1 : (opp_rung<0?1:bicho_side);    /* la óptica del bicho (en autojuego: blancas de referencia) */
    double becon = (bside>0?econ_w:econ_b);                /* su economía de material en la partida */
    int bwin = (result*bside>0)?1:((result*bside<0)?-1:0);
    double k_end = bside>0?pb_end:pw_end;                 /* piezas del enemigo del bicho */
    double my_end_b = bside>0?pw_end:pb_end;             /* MIS piezas (las del bicho) */
    int    not_losing_mat = (my_end_b >= k_end);         /* no estoy en déficit material */
    double king_drive = not_losing_mat ? c->par[P_KHUNT]*(0.5+(16-k_end)/16.0)*aggro : 0.0;  /* ANSIA DE REY en vivo: SOLO si el ataque está justificado (no voy abajo en material). Antes empujaba en TODA partida violenta, también desde posiciones perdidas -> se tiraba al ataque y lo mataban */
    double p_target = clampd(13.5 + bwin*7.0 + tanh(becon*1.5)*4.0 + aggro*3.0 + king_drive*5 + (brilliant?4.0:0.0), 0, 25);  /* peso del ansia de rey a la MITAD (1.5 en vez de 3.0): instinto, no obsesión */
    c->placer += (p_target - c->placer)*0.45;              /* relajación rápida -> PULSA como la energía */
    c->placer = clampd(c->placer,0,25);
    /* ENTRENAR V AL RESULTADO REAL (idea de Nico): cada posición vivida se empuja hacia el desenlace de la
       partida (1 ganaron blancas / 0 negras / 0.5 tablas). Es regresión logística online (Montecarlo). V
       queda anclada a GANAR, no a ninguna creencia material; y los valores de pieza caen solos de sus pesos. */
    if(vn>0){
      double rw = result>0?1.0:(result<0?0.0:0.5);
      double lrv = 0.04;
      for(int k=0;k<vn;k++){ double v=vpredict(c,vbuf[k]); double err=rw-v;
        for(int j=0;j<48;j++) c->vw[j]+= lrv*(err*vbuf[k][j] - 1e-4*c->vw[j]);  /* paso + L2 chico (no explota) */
        c->vb += lrv*err; }
      read_piece_values(c);   /* releo los valores de pieza desde los pesos de V */
    }
    prune_memory(c);
  }
  if(!observe){ c->games++; if(result>0)c->wins++; else if(result<0)c->losses++; else c->draws++;
    if(brilliant&&result!=0) c->brilliancies++; }   /* el récord de partidas y el currículum solo cuentan lo que JUGÓ */
  else c->observed++;                                      /* partida MIRADA (no jugada) */
  c->dream=0;
  return result;
}
static int play_and_learn(Cell* c,int learning){ return play_and_learn_ex(c,learning,-1,1,0); }  /* AUTOJUEGO (modo viejo) */

/* puntaje CONTINUO de una partida vs azar: premia mate rápido y margen material,
   así la aptitud no se clava en el techo aunque siempre le gane al azar */
static double score_vs_random(Cell* c,int org_side){
  Pos p; startpos(&p);
  for(int ply=0;ply<120;ply++){ Move L[256]; int n=gen_moves(&p,L);
    if(n==0){ if(in_check(&p,p.side)) return (p.side==org_side)? -1.0 : (1.0 - ply*0.0015); else return 0.0; }
    Move m; if(p.side==org_side){int t; m=choose(c,&p,0,&t);} else m=L[rndi(n)];
    Pos nx; make_move(&p,&m,&nx); p=nx;
    if(ply==119){ double d=(piece_count(&p,1)-piece_count(&p,-1))*org_side; return tanh(d/6.0)*0.6; } }
  return 0.0;
}
/* aptitud = puntaje continuo medio vs azar (display); winrate = fracción no perdida (display) */
static void measure(Cell* c){ int save=c->eval_mode; c->eval_mode=1; double s=0; int nl=0,g=4;
  for(int i=0;i<g;i++){ double r=score_vs_random(c,i%2==0?1:-1); s+=r; if(r>=-0.0001)nl++; }
  c->fitness=s/g; c->winrate=(double)nl/g; c->e_target=clampd(12.0+c->fitness*8.0,3,23); c->eval_mode=save; }

/* ===================== SPARRING FIJO: vara externa que NO sube con el bicho =====================
   Un rival que respeta material (valores clásicos) con quiescencia de capturas, para no colgar piezas.
   No evoluciona: es una regla fija contra la cual medir la FUERZA REAL (a diferencia de la coevolución,
   que es una cinta de correr porque el campeón sube con el bicho). El bicho NO aprende durante el sparring. */
static const int SPARVAL[7]={0,100,320,330,500,900,0};   /* P,N,B,R,Q (rey no cuenta material) */
static int spar_eval(const Pos* p){ int s=0; for(int i=0;i<64;i++){int pc=p->b[i]; if(pc){int t=pc>0?pc:-pc; s+=(pc>0?1:-1)*SPARVAL[t];}} return p->side>0? s : -s; }
static int spar_quiesce(const Pos* p,int alpha,int beta,int qd){
  int stand=spar_eval(p); if(stand>=beta)return beta; if(stand>alpha)alpha=stand;
  if(qd<=0) return alpha;
  Move mv[256]; int n=gen_caps_legal(p,mv);                      /* SOLO capturas legales (ya solo usaba capturas; ahora no genera ni chequea el resto) */
  for(int i=0;i<n;i++){
    Pos nx; make_move(p,&mv[i],&nx); int sc=-spar_quiesce(&nx,-beta,-alpha,qd-1);
    if(sc>=beta)return beta; if(sc>alpha)alpha=sc; }
  return alpha; }
static int spar_search(const Pos* p,int depth,int alpha,int beta){
  Move mv[256]; int n=gen_moves(p,mv);
  if(n==0){ if(in_check(p,p->side)) return -29000-depth; return 0; }   /* mate / ahogado */
  if(depth<=0) return spar_quiesce(p,alpha,beta,4);
  int best=-100000;
  for(int i=0;i<n;i++){ Pos nx; make_move(p,&mv[i],&nx); int sc=-spar_search(&nx,depth-1,-beta,-alpha);
    if(sc>best)best=sc; if(best>alpha)alpha=best; if(alpha>=beta)break; }
  return best; }
static Move spar_best(const Pos* p,int depth){
  Move mv[256]; int n=gen_moves(p,mv); Move bm=mv[0]; int best=-100001;
  for(int i=0;i<n;i++){ Pos nx; make_move(p,&mv[i],&nx); int sc=-spar_search(&nx,depth-1,-100000,100000);
    if(sc>best){best=sc;bm=mv[i];} }
  return bm; }
/* ===================== ESCALERA DE APRENDIZAJE (currículum) =====================
   Rivales fijos de fuerza creciente. El bicho aprende jugándoles; al vencer un nivel, sube al siguiente.
   Cada nivel = profundidad del minimax + una probabilidad de jugar al azar (blunder) para los más blandos. */
#define NRUNG 7
static const int    RUNG_DEPTH[NRUNG]  ={1,    1,    1,    2,    2,    3,    4};
static const double RUNG_BLUNDER[NRUNG]={0.90, 0.70, 0.45, 0.22, 0.08, 0.0,  0.0};
static const int    RUNG_ELO[NRUNG]    ={400,  600,  800,  1050, 1250, 1450, 1650};  /* Elo aprox del nivel */
static Move ladder_move(const Pos* p,int rung){
  if(rung<0)rung=0; if(rung>=NRUNG)rung=NRUNG-1;
  Move L[256]; int n=gen_moves(p,L);
  if(n==0){ Move z={0,0,0,0}; return z; }
  if(rnd()<RUNG_BLUNDER[rung]) return L[rndi(n)];    /* blunder: jugada al azar (nivel blando) */
  return spar_best(p,RUNG_DEPTH[rung]); }
/* una partida bicho vs rival fijo; devuelve +1/0/-1 desde la óptica del bicho */
static int sparring_game(Cell* c,int bicho_side,int oppdepth){
  Pos p; startpos(&p); int sv=c->eval_mode; c->eval_mode=1; int result=0;
  for(int ply=0; ply<200; ply++){
    Move L[256]; int n=gen_moves(&p,L);
    if(n==0){ if(in_check(&p,p.side)) result=-p.side; else result=0; break; }
    Move m;
    if(p.side==bicho_side){ int term; m=choose(c,&p,0,&term); if(term){ if(in_check(&p,p.side))result=-p.side; else result=0; break; } }
    else m=spar_best(&p,oppdepth);
    Pos nx; make_move(&p,&m,&nx); p=nx;
    if(ply==199){ int d=piece_count(&p,1)-piece_count(&p,-1); result=d>1?1:(d<-1?-1:0); } }
  c->eval_mode=sv;
  return result==0?0:(result==bicho_side?1:-1); }
/* ESCALERA DE SPARRING para estimar ELO: 3 rivales fijos (profundidad creciente) con un Elo asumido
   (anclas FIJAS, recalibrables). Del puntaje del bicho contra cada uno despejo su Elo con la logística,
   y combino las rampas pesando más la que da resultado cerca del 50% (la medición más confiable). */
#define ELO_RUNGS 3
static const int SPAR_DEPTHS[ELO_RUNGS]={1,2,3};
static const int SPAR_ELO[ELO_RUNGS]   ={800,1100,1400};   /* Elo asumido del minimax por profundidad */
#define SPAR_RUNG_GAMES 2
#define SPAR_DEPTH (SPAR_DEPTHS[ELO_RUNGS-1])               /* la rampa más dura, para mostrar */
static void measure_sparring(Cell* c){
  int tw=0,td=0,tl=0; double elosum=0, wsum=0;
  for(int r=0;r<ELO_RUNGS;r++){
    int w=0,d=0,l=0;
    for(int g=0; g<SPAR_RUNG_GAMES; g++){ int side=(g%2==0)?1:-1; int res=sparring_game(c,side,SPAR_DEPTHS[r]);
      if(res>0)w++; else if(res<0)l++; else d++; }
    tw+=w; td+=d; tl+=l;
    double s=(w+0.5*d)/SPAR_RUNG_GAMES;
    c->spar_rung_sc[r] = (c->spar_games==0) ? s : c->spar_rung_sc[r]*0.7 + s*0.3;   /* puntaje POR RIVAL (media móvil), alimenta las barras del dashboard */
    double sc=s<0.05?0.05:(s>0.95?0.95:s);                 /* evita Elo infinito en saturación */
    double elo=SPAR_ELO[r] + 400.0*log10(sc/(1.0-sc));     /* despeje logístico del Elo del bicho */
    double wgt=1.0-2.0*fabs(s-0.5); if(wgt<0.05)wgt=0.05;  /* más confiable cerca del 50% */
    elosum+=elo*wgt; wsum+=wgt;
  }
  int ng=ELO_RUNGS*SPAR_RUNG_GAMES;
  c->spar_games+=ng; c->spar_wins+=tw; c->spar_draws+=td; c->spar_losses+=tl;
  double sc=(tw+0.5*td)/ng;
  c->spar_score = c->spar_games<=ng ? sc : c->spar_score*0.8 + sc*0.2;
  double elo = wsum>0? elosum/wsum : 1000.0;
  c->spar_elo = c->spar_elo<1.0 ? elo : c->spar_elo*0.85 + elo*0.15;   /* media móvil del Elo */
  if(c->spar_elo > c->spar_elo_peak) c->spar_elo_peak = c->spar_elo;   /* se queda con el Elo más alto alcanzado */
}
/* CONTROLADOR DEL CURRÍCULUM: registra el resultado vs el nivel actual y sube/baja la vara.
   result01: 1=ganó, 0.5=tablas, 0=perdió (desde la óptica del bicho). */
static void school_step(Cell* c,double result01){
  c->school_games++;
  c->school_score = c->school_score<=0.0 && c->school_games<=1 ? result01
                  : c->school_score*0.95 + result01*0.05;          /* media móvil vs el nivel actual */
  if(c->school_score>0.60 && c->school_games>40 && c->school_rung<NRUNG-1){
    c->school_rung++; c->school_score=0.45; c->school_games=0;       /* venció el nivel -> SUBE la vara */
    logevt("ESCUELA","subió a nivel %d (Elo aprox %d)",c->school_rung,RUNG_ELO[c->school_rung]);
  } else if(c->school_score<0.15 && c->school_games>40 && c->school_rung>0){
    c->school_rung--; c->school_score=0.45; c->school_games=0;       /* lo aplastan -> BAJA un escalón */
    logevt("ESCUELA","bajó a nivel %d",c->school_rung);
  }
}

/* ============================ ESTUDIO DE MAESTROS (.pgn) ================== */
static int san_to_move(const Pos* p,const char* san,Move* out){
  char s[16]; int k=0; for(const char* c=san;*c&&k<15;c++){ if(*c=='+'||*c=='#'||*c=='!'||*c=='?')continue; s[k++]=*c; } s[k]=0;
  if(k==0)return 0; Move L[256]; int n=gen_moves(p,L);
  if(!strcmp(s,"O-O")||!strcmp(s,"0-0")){for(int i=0;i<n;i++)if(L[i].flag==3){*out=L[i];return 1;}return 0;}
  if(!strcmp(s,"O-O-O")||!strcmp(s,"0-0-0")){for(int i=0;i<n;i++)if(L[i].flag==4){*out=L[i];return 1;}return 0;}
  int piece=P,promo=0,len=k;
  if(len>=2&&s[len-2]=='='){char pp=s[len-1]; promo=pp=='Q'?Q:pp=='R'?R:pp=='B'?B:N; len-=2; s[len]=0;}
  int idx=0; if(s[0]=='N'||s[0]=='B'||s[0]=='R'||s[0]=='Q'||s[0]=='K'){piece=s[0]=='N'?N:s[0]=='B'?B:s[0]=='R'?R:s[0]=='Q'?Q:K; idx=1;}
  int dto=-1; if(len>=2&&s[len-1]>='1'&&s[len-1]<='8'&&s[len-2]>='a'&&s[len-2]<='h')dto=sq_of(s[len-2]-'a',s[len-1]-'1');
  if(dto<0)return 0; int dfile=-1,drank=-1;
  for(int i=idx;i<len-2;i++){if(s[i]>='a'&&s[i]<='h')dfile=s[i]-'a'; else if(s[i]>='1'&&s[i]<='8')drank=s[i]-'1';}
  for(int i=0;i<n;i++){ if(L[i].to!=dto||abs(p->b[L[i].from])!=piece)continue;
    if(promo&&L[i].promo!=promo)continue; if(!promo&&L[i].promo&&L[i].promo!=Q)continue;
    if(dfile>=0&&file_of(L[i].from)!=dfile)continue; if(drank>=0&&rank_of(L[i].from)!=drank)continue;
    *out=L[i]; return 1; }
  return 0;
}
/* APRENDE UNA partida desde el cursor *pcc (lo avanza). El bicho OBSERVA: percibe cada posición de maestro,
   refuerza conceptos con el resultado, y ENTRENA V (las partidas de maestro son datos reales de oro).
   Devuelve 1 si aprendió una partida, 0 si no quedaba ninguna. */
static double vpredict(Cell* c,const float* x);   /* fwd */
static void read_piece_values(Cell* c);            /* fwd */
static int learn_one_pgn_game(Cell* c, char** pcc){
  char* cc=*pcc; int result=0,have=0; Pos p; startpos(&p); int pidx[600],pside[600],hn=0;
  static float vbuf[300][48]; int vn=0;
  while(*cc){ if(*cc=='['){ char* nl=strchr(cc,'\n'); if(!nl){cc+=strlen(cc);break;}
      if(strstr(cc,"Result")&&strstr(cc,"1-0")&&cc<nl)result=1; if(strstr(cc,"Result")&&strstr(cc,"0-1")&&cc<nl)result=-1; cc=nl+1; continue;}
    if(*cc=='\n'||*cc==' '||*cc=='\r'){cc++;continue;} break; }
  while(*cc){ if(*cc=='['){break;} if(*cc=='{'){char* e=strchr(cc,'}'); if(!e)break; cc=e+1; continue;}
    if(*cc=='('){char* e=strchr(cc,')'); if(!e){cc++;continue;} cc=e+1; continue;}
    if(*cc=='$'){while(*cc&&*cc!=' '&&*cc!='\n')cc++; continue;}
    if(*cc==' '||*cc=='\n'||*cc=='\r'||*cc=='\t'){cc++;continue;}
    char tok[24]; int tl=0; while(*cc&&*cc!=' '&&*cc!='\n'&&*cc!='\r'&&*cc!='\t'&&tl<23){tok[tl++]=*cc;cc++;} tok[tl]=0;
    if(!strcmp(tok,"1-0")){result=1;break;} if(!strcmp(tok,"0-1")){result=-1;break;}
    if(!strcmp(tok,"1/2-1/2")||!strcmp(tok,"*")){result=0;break;}
    char* dot=strchr(tok,'.'); char* san=dot?dot+1:tok; if(!*san)continue;
    if(*san>='0'&&*san<='9'&&!strchr(san,'-'))continue;
    Move m; if(san_to_move(&p,san,&m)){ float x[FEATD]; board_features(&p,x); int cr; int idx=perceive(c,&p,x,&cr);
      if(hn<600){pidx[hn]=idx;pside[hn]=p.side;hn++;} if(vn<300){for(int j=0;j<48;j++)vbuf[vn][j]=x[j];vn++;} Pos nx; make_move(&p,&m,&nx); p=nx; have=1; }
    else break;
  }
  if(have){ for(int i=0;i<hn;i++){ int S=pside[i]; float te=(float)(result*S>0?0.5:(result==0?0:-0.3));
      float tp=(float)(result*S>0?0.7:0); reinforce(c,pidx[i],te,tp,1.0f); }  /* el maestro entrena conceptos/V, pero NO suma mérito: el mérito es el rendimiento PROPIO del bicho jugando, no la victoria del maestro */
    c->pgn_games++; c->pgn_pos+=hn;
    /* ENTRENA V con la partida de maestro, pero SUAVE (lrv bajo): los maestros suelen ser balanceados
       (señal de material débil); los VALORES de pieza los anclan las partidas decisivas de la escuela.
       Acá V se refina sobre todo en lo posicional sin lavar esos valores. */
    double rw = result>0?1.0:(result<0?0.0:0.5); double lrv=0.015;
    for(int k=0;k<vn;k++){ double v=vpredict(c,vbuf[k]); double err=rw-v;
      for(int j=0;j<48;j++) c->vw[j]+= lrv*(err*vbuf[k][j] - 1e-4*c->vw[j]); c->vb += lrv*err; }
    read_piece_values(c);
  }
  while(*cc==' '||*cc=='\n'||*cc=='\r'||*cc=='\t')cc++;
  *pcc=cc; return have;
}
static void study_pgn(Cell* c,const char* path){   /* BATCH: estudia un archivo entero (siembra inicial) */
  FILE* f=fopen(path,"rb"); if(!f)return; static char buf[1<<20]; size_t rd=fread(buf,1,sizeof(buf)-1,f); buf[rd]=0; fclose(f);
  char* cc=buf; while(*cc){ if(!learn_one_pgn_game(c,&cc) && !*cc) break; }
}
/* ===== GOTEO / DRIP: el bicho observa unas pocas partidas de maestro por tick (cursor persistente) ===== */
static char DRIP_BUF[1<<20]; static char* DRIP_CC=NULL;
static char DRIP_FILES[64][300]; static int DRIP_NFILES=-1, DRIP_FILEIDX=0;
static void drip_scan_files(Cell* c){ DRIP_NFILES=0; char dir[1200]; snprintf(dir,sizeof dir,"%s/maestros",c->nido);
  DIR* d=opendir(dir); if(!d)return; struct dirent* e;
  while((e=readdir(d))&&DRIP_NFILES<64){ if(strstr(e->d_name,".pgn")||strstr(e->d_name,".PGN")){
    snprintf(DRIP_FILES[DRIP_NFILES],300,"%s/%s",dir,e->d_name); DRIP_NFILES++; } } closedir(d); }
static int drip_load_next_file(Cell* c){ if(DRIP_NFILES<0)drip_scan_files(c); if(DRIP_NFILES==0)return 0;
  const char* fp=DRIP_FILES[DRIP_FILEIDX%DRIP_NFILES]; DRIP_FILEIDX++;
  FILE* f=fopen(fp,"rb"); if(!f){DRIP_CC=NULL;return 0;} size_t rd=fread(DRIP_BUF,1,sizeof(DRIP_BUF)-1,f); DRIP_BUF[rd]=0; fclose(f); DRIP_CC=DRIP_BUF; return 1; }
static void study_pgn_drip(Cell* c,int ngames){
  c->dream=0; float sv=CUR_REALITY; CUR_REALITY=0.95f;   /* el maestro es la experiencia MÁS real */
  int done=0, guard=0;
  while(done<ngames && guard<ngames*6+8){ guard++;
    if(!DRIP_CC || !*DRIP_CC){ if(!drip_load_next_file(c)) break; }   /* buffer vacío -> próximo archivo (cicla) */
    if(learn_one_pgn_game(c,&DRIP_CC)) done++;
  }
  CUR_REALITY=sv;
}
/* CANAL REAL VIVO (camino 3): re-estudia todos los .pgn de nido/maestros. Corre con dream=0, así
   confirma ideas soñadas que resultaron sólidas y mantiene al núcleo creciendo de ajedrez real. */
#define RESTUDY_EVERY 100000   /* ticks entre re-estudios de maestros (subilo si te mete hipo, bajalo para más confirmación) */
#ifndef GAMES_PER_DRIP
#define GAMES_PER_DRIP 2     /* partidas de maestro que el bicho OBSERVA por tick (goteo). Sin minimax => rápido. Subilo para más estudio/tick, bajalo si va lento. */
#endif
#ifndef SELFPLAY_FROM
#define SELFPLAY_FROM 0   /* hasta este tick SOLO estudia maestros (fundación real); recién después juega su propio ajedrez. Jugar antes de tener base mete ruido. */
#endif
static void study_all_masters(Cell* c){
  c->dream=0;   /* asegura modo REAL: lo que toque acá cuenta como dato real, no imaginado */
  float sv=CUR_REALITY; CUR_REALITY=0.95f;   /* el maestro es la experiencia MÁS real */
  char dir[1200]; snprintf(dir,sizeof dir,"%s/maestros",c->nido); DIR* d=opendir(dir); if(!d){CUR_REALITY=sv;return;}
  struct dirent* e; while((e=readdir(d))){ if(strstr(e->d_name,".pgn")||strstr(e->d_name,".PGN")){
    char fp[1500]; snprintf(fp,sizeof fp,"%s/%s",dir,e->d_name); long g0=c->pgn_games; study_pgn(c,fp);
    logevt("ESTUDIO","estudió %s (+%ld partidas de maestros)",e->d_name,c->pgn_games-g0); } } closedir(d);
  CUR_REALITY=sv;
}

/* ============================ GENOMA / PERSISTENCIA ======================= */
static char bch(int b){return "ACGT"[b&3];}
typedef struct { double par[NPARAM]; Gene gene[GMAX]; int ng; } Pheno;
static Gene POOL[GPOOL]; static int POOLIX[GPOOL];   /* scratch: rankea el pool frío y elige el caliente */
static void decode_one_gene(const char* b, Gene* gg){
  gg->on   = bval(b[0])>=2;                          /* G/T => expresado; el resto no codificante */
  gg->type = (bval(b[1])*4+bval(b[2]))%7;            /* 0=cualquier pieza, 1-6=tipo */
  int sd   = bval(b[3]); gg->side = sd<2?0:(sd==2?1:2); /* 0=mías, 1=del rival, 2=cualquiera */
  gg->ref  = bval(b[4]);                              /* 0=mi rey, 1=rey rival, 2=centro, 3=todo */
  gg->radius = bval(b[5])+1;                          /* 1-4 */
  int tr=0; for(int k=0;k<6;k++) tr=(tr<<2)|bval(b[6+k]); gg->trig = tr & ((1<<NCOND)-1); /* CUÁNDO despierta */
  int pv = ((bval(b[12])*4+bval(b[13]))*4+bval(b[14]))*4+bval(b[15]); /* 4 bases => 0-255 => PRIORIDAD 1-100 fina */
  gg->prio = 1 + (pv*99)/255;
}
static void decode_pheno(const char* g,int glen,Pheno* ph,int* hotmap,int* total){
  static const double LO[NPARAM]={0.05,0.15,0.985,1,0.0,0.0,0.0,0.0,0.05,0.0,  0.0,0.0,0.0,0.2,  0.30};
  static const double HI[NPARAM]={0.45,0.55,0.9995,3,1.2,1.5,2.0,0.6,0.5,1.0,  0.9,0.9,1.2,1.2,  1.00};
  for(int i=0;i<NPARAM;i++){int v=0;for(int k=0;k<3;k++)v=v*4+bval(g[i*3+k]); ph->par[i]=LO[i]+(v/63.0)*(HI[i]-LO[i]);}
  int n=(glen-GENE_OFF)/GENE_LEN; if(n<0)n=0; if(n>GPOOL)n=GPOOL;   /* el POOL puede ser ENORME, sin techo de 256 */
  for(int i=0;i<n;i++){ decode_one_gene(&g[GENE_OFF+i*GENE_LEN], &POOL[i]); POOLIX[i]=i; }
  /* VALORACIÓN POR PIEZA (diseño de Nico): garantizo UN sentido por cada tipo de pieza (peón=1..dama=5),
     con el `type` CLAVADO y siempre expresado/caliente. El resto de sus campos (referencia/radio/cuándo
     despierta) SÍ evoluciona desde el genoma, y el reemplazo-si-mejor lo hace la evolución normal (adopta
     solo si no baja la fuerza real). No coexisten dos iguales: cada slot es un tipo distinto. */
  if(n<5){ for(int i=n;i<5;i++){ memset(&POOL[i],0,sizeof(Gene)); POOL[i].side=2; POOL[i].ref=3; POOL[i].radius=2; POOL[i].trig=0; POOLIX[i]=i; } n=5; }
  for(int t=1;t<=5;t++){ Gene* vg=&POOL[t-1]; vg->type=t; vg->on=1; vg->prio=100; }
  int hot = n>GMAX?GMAX:n;                                           /* SET CALIENTE: como mucho GMAX despiertos */
  if(n>GMAX){ /* selección parcial: los GMAX de MAYOR PRIORIDAD pasan al frente (los demás duermen) */
    for(int a=0;a<GMAX;a++){ int best=a;
      for(int b2=a+1;b2<n;b2++) if(POOL[POOLIX[b2]].prio>POOL[POOLIX[best]].prio) best=b2;
      int t=POOLIX[a]; POOLIX[a]=POOLIX[best]; POOLIX[best]=t; } }
  for(int i=0;i<hot;i++){ ph->gene[i]=POOL[POOLIX[i]]; if(hotmap)hotmap[i]=POOLIX[i]; }
  ph->ng=hot; if(total)*total=n;
}
/* aplica el genoma. El costo por jugada lo acota el selector (KSLOT despiertos), no el tamaño. */
static void apply_genes(const Pheno* ph){
  int n=ph->ng>GMAX?GMAX:ph->ng; for(int i=0;i<n;i++)GENES[i]=ph->gene[i]; NGENES=n;
  GENOME_VER++;                               /* invalida los presets: se recalculan perezosos al reaparecer su firma */
  unsigned h=2166136261u;                     /* huella FNV-1a estable de los genes (para presets en disco) */
  for(int i=0;i<n;i++){ Gene* g=&GENES[i]; int v=g->on|(g->type<<1)|(g->side<<4)|(g->ref<<6)|(g->radius<<8)|(g->trig<<12);
    h=(h^(unsigned)(v&0xff))*16777619u; h=(h^(unsigned)((v>>8)&0xff))*16777619u; h=(h^(unsigned)((v>>16)&0xff))*16777619u; }
  GENOME_HASH=h;
}
static void decode_params(Cell* c){ Pheno ph; decode_pheno(c->genome,c->glen,&ph,GHOT,&NGENE_TOTAL);
  memcpy(c->par,ph.par,sizeof c->par); apply_genes(&ph);
  NGCOLD=0;                                                     /* arma el POOL FRÍO (los que no entraron al caliente) */
  for(int i=GMAX;i<NGENE_TOTAL && NGCOLD<GPOOL;i++){ GCOLD[NGCOLD]=POOL[POOLIX[i]]; GCOLD_IX[NGCOLD]=POOLIX[i]; NGCOLD++; } }
/* EL USO AJUSTA LA PRIORIDAD (de TODO el pool, caliente y despertado): sube proporcional al uso,
   baja fijo si estuvo ocioso. Los útiles ascienden, los inútiles se hunden y duermen; un raro-pero-clave
   se mantiene bajo pero NUNCA muere, porque despierta por contexto cuando aparece su jugada. */
static void adjust_gene_priorities(Cell* c){
  int maxf=1; for(int i=0;i<NGENE_TOTAL;i++) if(GFIRE_POOL[i]>maxf) maxf=GFIRE_POOL[i];
  float rmax=(float)sqrt((double)maxf);
  for(int i=0;i<NGENE_TOTAL;i++){ int off=GENE_OFF+i*GENE_LEN;
    int pv=((bval(c->genome[off+12])*4+bval(c->genome[off+13]))*4+bval(c->genome[off+14]))*4+bval(c->genome[off+15]); /* 0-255 */
    float p=1.0f+(pv*99)/255.0f;
    /* objetivo = CUOTA de uso relativa al gen más usado (raíz para repartir el medio); el ocioso -> 1 */
    float target=1.0f + 99.0f*((float)sqrt((double)GFIRE_POOL[i])/rmax);
    p += PRIO_BLEND*(target - p);                 /* el uso la empuja hacia su cuota: spread rico, sin saturar */
    if(p<1)p=1; if(p>100)p=100;
    int npv=(int)((p-1.0f)*255.0f/99.0f+0.5f); if(npv<0)npv=0; if(npv>255)npv=255;
    c->genome[off+12]=bch((npv>>6)&3); c->genome[off+13]=bch((npv>>4)&3);
    c->genome[off+14]=bch((npv>>2)&3); c->genome[off+15]=bch(npv&3); }
  memset(GFIRE_POOL,0,sizeof GFIRE_POOL);
  decode_params(c);                                            /* refresca caliente y frío con las nuevas prioridades */
}
static void seed_genome(Cell* c){ c->glen=0; for(int i=0;i<NPARAM;i++){int v=31+rndi(8)-4; if(i==P_KHUNT)v=40+rndi(8); /* nace con ANSIA DE REY moderada (~0.75), no suicida: la evolución la ajusta */
    c->genome[c->glen++]=bch((v>>4)&3);c->genome[c->glen++]=bch((v>>2)&3);c->genome[c->glen++]=bch(v&3);}
  for(int gi=0;gi<2;gi++)for(int k=0;k<GENE_LEN;k++)c->genome[c->glen++]=bch(rndi(4)); /* nace con 2 sentidos */
  c->genome[c->glen]=0; decode_params(c); }
static void save_genome(Cell* c){char p[1200];snprintf(p,sizeof p,"%s/genoma.dna",c->nido);char t[1300];snprintf(t,sizeof t,"%s.tmp",p);
  FILE* f=fopen(t,"w"); if(!f)return; fprintf(f,"# organismo ajedrecista (gen %ld) layout=%d\n>chr1\n",c->generation,NPARAM);
  for(int i=0;i<c->glen;i++){fputc(c->genome[i],f); if((i+1)%60==0)fputc('\n',f);} fputc('\n',f); fclose(f); remove(p); rename(t,p); }
static int load_genome(Cell* c){char p[1200];snprintf(p,sizeof p,"%s/genoma.dna",c->nido);FILE* f=fopen(p,"r");if(!f)return 0;
  int layout=14;                          /* genomas SIN marca = layout viejo de 14 instintos */
  c->glen=0;int ch;while((ch=fgetc(f))!=EOF&&c->glen<GENOME_CAP-1){
    if(ch=='#'){ char ln[160]; int li=0; while((ch=fgetc(f))!=EOF&&ch!='\n'){ if(li<159)ln[li++]=(char)ch; } ln[li]=0;
                 char* lp=strstr(ln,"layout="); if(lp)layout=atoi(lp+7); continue; }
    if(ch=='>'){ while((ch=fgetc(f))!=EOF&&ch!='\n'); continue; }
    if(ch=='A'||ch=='C'||ch=='G'||ch=='T')c->genome[c->glen++]=ch;}c->genome[c->glen]=0;fclose(f);
  /* MIGRACIÓN: si el ADN trae menos instintos que hoy, inserto los nuevos (ANSIA DE REY, sembrada ALTO)
     en su posición, SIN desalinear los sentidos ya evolucionados. Conserva percepción y carácter viejos. */
  if(layout<NPARAM){ int ins=(NPARAM-layout)*3, base=layout*3;
    if(base<=c->glen && c->glen+ins < GENOME_CAP-1){
      memmove(&c->genome[base+ins], &c->genome[base], (size_t)(c->glen-base));
      for(int q=0;q<ins;q+=3){ c->genome[base+q]='G'; c->genome[base+q+1]='G'; c->genome[base+q+2]='A'; } /* v=40/63 ~ ansia MODERADA (coherente con seed_genome) */
      c->glen+=ins; c->genome[c->glen]=0; } }
  if(c->glen<NPARAM*3)return 0;
  while(c->glen < GENE_OFF + 2*GENE_LEN) c->genome[c->glen++]=bch(rndi(4));  /* genoma viejo: despiertan 2 sentidos */
  c->genome[c->glen]=0; decode_params(c); return 1; }

/* ================================ BITÁCORA (cap 5MB) ====================== */
static FILE* BIT=NULL; static char BITPATH[1200]; static Cell* GC=NULL;
static void log_cap(void){ struct stat st; if(stat(BITPATH,&st)==0&&st.st_size>5L*1024*1024){
    FILE* f=fopen(BITPATH,"rb"); if(!f)return; fseek(f,st.st_size/2,SEEK_SET); int ch; while((ch=fgetc(f))!=EOF&&ch!='\n');
    static char buf[3L*1024*1024]; size_t rd=fread(buf,1,sizeof buf,f); fclose(f);
    if(BIT)fclose(BIT); FILE* w=fopen(BITPATH,"wb"); if(w){fwrite(buf,1,rd,w);fclose(w);} BIT=fopen(BITPATH,"a"); } }
static void logevt(const char* tag,const char* fmt,...){ char msg[200];va_list ap;va_start(ap,fmt);vsnprintf(msg,sizeof msg,fmt,ap);va_end(ap);
  if(BIT&&GC){time_t now=time(NULL);char ts[16];struct tm* tm=localtime(&now);strftime(ts,sizeof ts,"%H:%M:%S",tm);
    fprintf(BIT,"%s\tt=%ld\tgen=%ld\t[%s]\t%s\n",ts,GC->age,GC->generation,tag,msg);fflush(BIT);} }

static void save_stats(Cell* c){ char p[1200];snprintf(p,sizeof p,"%s/memoria/_stats.txt",c->nido);FILE* f=fopen(p,"w"); if(!f)return;
  fprintf(f,"generation %ld\nadoptions %ld\ngames %ld\nwins %ld\ndraws %ld\nlosses %ld\nbrilliancies %ld\ndreams %ld\ncoevo_chal %ld\ncoevo_win %ld\ncoevo_veto %ld\nspar_games %ld\nspar_wins %ld\nspar_draws %ld\nspar_losses %ld\nspar_elo %ld\n",
    c->generation,c->adoptions,c->games,c->wins,c->draws,c->losses,c->brilliancies,c->dreams,c->coevo_chal,c->coevo_win,c->coevo_veto,
    c->spar_games,c->spar_wins,c->spar_draws,c->spar_losses,(long)(c->spar_elo+0.5)); fprintf(f,"school_rung %d\nobserved %ld\n",c->school_rung,c->observed);
  fprintf(f,"pval1 %ld\npval2 %ld\npval3 %ld\npval4 %ld\npval5 %ld\n",(long)(c->pval[1]*1000),(long)(c->pval[2]*1000),(long)(c->pval[3]*1000),(long)(c->pval[4]*1000),(long)(c->pval[5]*1000));
  fclose(f); }
static void load_stats(Cell* c){ char p[1200];snprintf(p,sizeof p,"%s/memoria/_stats.txt",c->nido);FILE* f=fopen(p,"r"); if(!f)return;
  char key[32]; long v; while(fscanf(f,"%31s %ld",key,&v)==2){
    if(!strcmp(key,"generation"))c->generation=v; else if(!strcmp(key,"adoptions"))c->adoptions=v;
    else if(!strcmp(key,"games"))c->games=v; else if(!strcmp(key,"wins"))c->wins=v; else if(!strcmp(key,"draws"))c->draws=v;
    else if(!strcmp(key,"losses"))c->losses=v; else if(!strcmp(key,"brilliancies"))c->brilliancies=v; else if(!strcmp(key,"dreams"))c->dreams=v;
    else if(!strcmp(key,"coevo_chal"))c->coevo_chal=v; else if(!strcmp(key,"coevo_win"))c->coevo_win=v; else if(!strcmp(key,"coevo_veto"))c->coevo_veto=v;
    else if(!strcmp(key,"spar_games"))c->spar_games=v; else if(!strcmp(key,"spar_wins"))c->spar_wins=v; else if(!strcmp(key,"spar_draws"))c->spar_draws=v; else if(!strcmp(key,"spar_losses"))c->spar_losses=v; else if(!strcmp(key,"spar_elo"))c->spar_elo=(double)v;
    else if(!strcmp(key,"school_rung"))c->school_rung=(int)v; else if(!strcmp(key,"observed"))c->observed=v;
    else if(!strcmp(key,"pval1"))c->pval[1]=v/1000.0; else if(!strcmp(key,"pval2"))c->pval[2]=v/1000.0;
    else if(!strcmp(key,"pval3"))c->pval[3]=v/1000.0; else if(!strcmp(key,"pval4"))c->pval[4]=v/1000.0;
    else if(!strcmp(key,"pval5"))c->pval[5]=v/1000.0; }
  if(c->spar_games>0) c->spar_score=(c->spar_wins+0.5*c->spar_draws)/c->spar_games;   /* deriva la fuerza real de lo acumulado */
  fclose(f); }
/* PESOS DE V (48 + sesgo) en archivo aparte: son floats, no caben en el _stats de enteros */
static void save_vhead(Cell* c){ char p[1200];snprintf(p,sizeof p,"%s/memoria/_vhead.txt",c->nido);FILE* f=fopen(p,"w");if(!f)return;
  for(int j=0;j<48;j++)fprintf(f,"%.6f ",c->vw[j]); fprintf(f,"\n%.6f\n",c->vb); fclose(f); }
static void load_vhead(Cell* c){ char p[1200];snprintf(p,sizeof p,"%s/memoria/_vhead.txt",c->nido);FILE* f=fopen(p,"r");if(!f)return;
  for(int j=0;j<48;j++){ if(fscanf(f,"%f",&c->vw[j])!=1)break; } double b; if(fscanf(f,"%lf",&b)==1)c->vb=b; fclose(f); }
static void save_presets(Cell* c){
  char dir[1200]; snprintf(dir,sizeof dir,"%s/presets",c->nido); MKDIR(dir);
  for(int sig=0; sig<(1<<NCOND); sig++){ Preset* pr=&PRESET[sig];
    if(pr->ver!=GENOME_VER || pr->n<=0) continue;                 /* solo las firmas que de verdad ocurrieron */
    char p[1300]; snprintf(p,sizeof p,"%s/s%04d.dat",dir,sig); FILE* f=fopen(p,"wb"); if(!f)continue;
    fwrite(&GENOME_HASH,sizeof GENOME_HASH,1,f); fwrite(&NGENES,sizeof NGENES,1,f);
    fwrite(&pr->n,sizeof pr->n,1,f); fwrite(pr->idx,sizeof(short),pr->n,f); fclose(f); }
}
static void load_presets(Cell* c){
  for(int sig=0; sig<(1<<NCOND); sig++){ char p[1300]; snprintf(p,sizeof p,"%s/presets/s%04d.dat",c->nido,sig);
    FILE* f=fopen(p,"rb"); if(!f)continue; unsigned h; int ng; short n;
    if(fread(&h,sizeof h,1,f)==1 && h==GENOME_HASH && fread(&ng,sizeof ng,1,f)==1 && ng==NGENES
       && fread(&n,sizeof n,1,f)==1 && n>0 && n<=KSLOT){
      Preset* pr=&PRESET[sig]; if(fread(pr->idx,sizeof(short),n,f)==(size_t)n){
        int ok=1; for(int j=0;j<n;j++) if(pr->idx[j]<0||pr->idx[j]>=NGENES){ok=0;break;}
        if(ok){ pr->n=n; pr->ver=GENOME_VER; } } }
    fclose(f); }
}
static void save_nucleo(Cell* c){
  char p[1200]; snprintf(p,sizeof p,"%s/memoria/nucleo.dat",c->nido);
  FILE* f=fopen(p,"wb"); if(!f)return; int ver=MEM_VER,cnt=0;
  for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive&&c->mem[i].core)cnt++;
  fwrite(&ver,sizeof ver,1,f); fwrite(&cnt,sizeof cnt,1,f);
  for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive&&c->mem[i].core) fwrite(&c->mem[i],sizeof(Concept),1,f);
  fclose(f);
}
static void load_nucleo(Cell* c){
  char p[1200]; snprintf(p,sizeof p,"%s/memoria/nucleo.dat",c->nido);
  FILE* f=fopen(p,"rb"); if(!f)return; int ver=0,cnt=0;
  if(fread(&ver,sizeof ver,1,f)!=1||ver!=MEM_VER){fclose(f);return;}
  if(fread(&cnt,sizeof cnt,1,f)!=1){fclose(f);return;}
  for(int k=0;k<cnt;k++){ Concept t; if(fread(&t,sizeof t,1,f)!=1)break;
    if(find_by_gid(c,t.gid)>=0) continue; int s=alloc_slot(c); if(s<0)break;
    c->mem[s]=t; c->mem[s].alive=1; c->mem[s].core=1; if(t.gid>=c->next_gid)c->next_gid=t.gid+1; }
  fclose(f);
}
/* GUARDA DE VERSIÓN: si el layout del Concept cambió (MEM_VER distinto), la memoria vieja en disco es
   ILEGIBLE. En vez de rechazarla en silencio (y correr roto, medio-reconstruido), la limpia y arranca
   fresca. Conserva genoma/V/stats (que no dependen del layout binario del Concept). */
static void version_guard(Cell* c){
  char vp[1200]; snprintf(vp,sizeof vp,"%s/memoria/_ver.dat",c->nido);
  int disk_ver=0; FILE* f=fopen(vp,"rb"); if(f){ if(fread(&disk_ver,sizeof disk_ver,1,f)!=1)disk_ver=0; fclose(f); }
  if(disk_ver==MEM_VER) return;
  char dir[1200]; snprintf(dir,sizeof dir,"%s/memoria",c->nido); DIR* d=opendir(dir); int nk=0;
  if(d){ struct dirent* e; while((e=readdir(d))){ const char* n=e->d_name;
      if(strncmp(n,"mod_",4)==0 || strcmp(n,"nucleo.dat")==0 || strcmp(n,"_index.dat")==0){
        char p[1400]; snprintf(p,sizeof p,"%s/%s",dir,n); remove(p); nk++; } } closedir(d); }
  f=fopen(vp,"wb"); if(f){ int v=MEM_VER; fwrite(&v,sizeof v,1,f); fclose(f); }
  if(disk_ver!=0) logevt("NIDO","memoria incompatible (cambió el layout): limpié %d archivos, arranca fresca",nk);
}
static void save_all_memory(Cell* c){ compete_nucleo(c);   /* capa/poda el núcleo ANTES de guardar -> el disco refleja el estado limpio */
  flush_all(c);                                            /* solo módulos CARGADOS y sucios: nunca pisa lo paginado en disco */
  save_nucleo(c);                                          /* el núcleo, completo, en su propio archivo */
  char p[1200];snprintf(p,sizeof p,"%s/memoria/_index.dat",c->nido);FILE* f=fopen(p,"wb"); if(f){fwrite(&c->next_gid,sizeof c->next_gid,1,f);fclose(f);} save_stats(c); save_vhead(c); }

/* ============================ PARTIDA CONTRA EL USUARIO ==================== */
static Pos GAME; static int GAME_ACTIVE=0,USER_SIDE=1,GAME_RESULT=2; static char LASTMOVE[8]="";
static void board_to_str(const Pos* p,char* s){const char* sym=".PNBRQK"; for(int i=0;i<64;i++){int pc=p->b[i];char ch=sym[abs(pc)];if(pc<0)ch+=32;s[i]=pc?ch:'.';} s[64]=0;}
static int apply_user_move(const char* mv){ if(!GAME_ACTIVE||GAME.side!=USER_SIDE)return 0;
  int from=sq_of(mv[0]-'a',mv[1]-'1'),to=sq_of(mv[2]-'a',mv[3]-'1'),promo=0;
  if(mv[4]){char pp=mv[4];promo=pp=='q'?Q:pp=='r'?R:pp=='b'?B:pp=='n'?N:0;}
  Move L[256];int n=gen_moves(&GAME,L); int org_before=piece_count(&GAME,-USER_SIDE);
  for(int i=0;i<n;i++) if(L[i].from==from&&L[i].to==to&&(L[i].promo==0||promo==0||L[i].promo==promo)){
    Pos nx;make_move(&GAME,&L[i],&nx);GAME=nx;snprintf(LASTMOVE,8,"%s",mv);
    if(GC&&piece_count(&GAME,-USER_SIDE)<org_before) GC->energy=clampd(GC->energy-0.8,0,25);  /* te la comiste */
    return 1;} return 0; }
static void org_reply(Cell* c){ if(!GAME_ACTIVE||GAME.side==USER_SIDE)return; int term; int sv=c->eval_mode; c->eval_mode=1;
  Move m=choose(c,&GAME,c->par[P_CREAT]*0.2,&term); c->eval_mode=sv;
  if(term){GAME_RESULT=in_check(&GAME,GAME.side)?(GAME.side==USER_SIDE?1:0):2;return;}
  char ms[6];ms[0]='a'+file_of(m.from);ms[1]='1'+rank_of(m.from);ms[2]='a'+file_of(m.to);ms[3]='1'+rank_of(m.to);ms[4]=m.promo?"  nbrq"[m.promo]:0;ms[m.promo?5:4]=0;
  int user_before=piece_count(&GAME,USER_SIDE);
  Pos nx;make_move(&GAME,&m,&nx);GAME=nx;snprintf(LASTMOVE,8,"%s",ms);
  if(piece_count(&GAME,USER_SIDE)<user_before) c->energy=clampd(c->energy+0.8,0,25);   /* comió: hambre saciada */
  Move L[256]; if(gen_moves(&GAME,L)==0)GAME_RESULT=in_check(&GAME,GAME.side)?(GAME.side==USER_SIDE?1:0):2; }
static void poll_command(Cell* c){ char p[1200];snprintf(p,sizeof p,"%s/comando.txt",c->nido);FILE* f=fopen(p,"r");if(!f)return;
  char line[64]; if(fgets(line,sizeof line,f)){char cmd[16],arg[16];arg[0]=0;
    if(sscanf(line,"%15s %15s",cmd,arg)>=1){
      if(!strcmp(cmd,"NUEVA")){startpos(&GAME);GAME_ACTIVE=1;GAME_RESULT=2;LASTMOVE[0]=0;USER_SIDE=!strcmp(arg,"negras")?-1:1;c->plan_len=0;
        logevt("PARTIDA","nueva partida; el usuario juega %s",USER_SIDE>0?"blancas":"negras"); if(GAME.side!=USER_SIDE)org_reply(c);}
      else if(!strcmp(cmd,"JUGADA")){ if(apply_user_move(arg)){Move L[256];
        if(gen_moves(&GAME,L)==0)GAME_RESULT=in_check(&GAME,GAME.side)?(GAME.side==USER_SIDE?0:1):2; else org_reply(c);} }
    } } fclose(f); FILE* w=fopen(p,"w"); if(w)fclose(w); }

/* ================================ TELEMETRÍA ============================== */
static void write_snapshot(Cell* c){ char p[1200];snprintf(p,sizeof p,"%s/snapshot.json",c->nido);char t[1300];snprintf(t,sizeof t,"%s.tmp",p);
  FILE* f=fopen(t,"w"); if(!f)return;
  int alive=0,core=0,imag=0,fatk=0,fdef=0,ndeep=0; double cmsum=0,cmmin=1e9; int cmn=0,spec=0;
  for(int i=0;i<c->nconcepts;i++)if(c->mem[i].alive){alive++;
    if(c->mem[i].core){core++; if(c->mem[i].pin_until>0)spec++; if(c->mem[i].merit_n>0){ cmsum+=c->mem[i].merit; if(c->mem[i].merit<cmmin)cmmin=c->mem[i].merit; cmn++; }}
    if(c->mem[i].born_dream)imag++;
    if(c->mem[i].deep)ndeep++; int k=c->mem[i].module_key; if(!c->mem[i].deep){ if(k>=1&&k<=36)fatk++; else if(k>=37&&k<=72)fdef++; }}
  double cmavg = cmn>0?cmsum/cmn:0.0; if(cmn==0)cmmin=0.0;
  char bs[80]; board_to_str(&GAME,bs); Move L[256]; int nm=GAME_ACTIVE?gen_moves(&GAME,L):0;
  fprintf(f,"{\n  \"tick\": %ld, \"generation\": %ld, \"immortal\": true,\n",c->age,c->generation);
  fprintf(f,"  \"energy\": %.2f, \"placer\": %.2f,\n",c->energy,c->placer);
  fprintf(f,"  \"winrate\": %.3f, \"fitness\": %.3f, \"games\": %ld, \"wins\": %ld, \"draws\": %ld, \"losses\": %ld, \"brilliancies\": %ld,\n",c->winrate,c->fitness,c->games,c->wins,c->draws,c->losses,c->brilliancies);
  fprintf(f,"  \"dreams\": %ld, \"lateral\": %ld, \"adoptions\": %ld,\n",c->dreams,c->lateral,c->adoptions);
  fprintf(f,"  \"coevolution\": {\"challenges\": %ld, \"wins\": %ld, \"vetoed\": %ld, \"last_margin\": %.2f},\n",
    c->coevo_chal,c->coevo_win,c->coevo_veto,c->coevo_last);
  { int rcur=0; for(int r=0;r<ELO_RUNGS;r++) if(c->spar_rung_sc[r]>=0.5) rcur=r;   /* rival más duro que aún empata-o-gana */
  fprintf(f,"  \"sparring\": {\"games\": %ld, \"wins\": %ld, \"draws\": %ld, \"losses\": %ld, \"score\": %.3f, \"elo\": %.0f, \"elo_peak\": %.0f, \"depth\": %d, \"rung_score\": [%.3f,%.3f,%.3f], \"rung_elo\": [%d,%d,%d], \"rung_cur\": %d},\n",
    c->spar_games,c->spar_wins,c->spar_draws,c->spar_losses,c->spar_score,c->spar_elo,c->spar_elo_peak,SPAR_DEPTH,
    c->spar_rung_sc[0],c->spar_rung_sc[1],c->spar_rung_sc[2],SPAR_ELO[0],SPAR_ELO[1],SPAR_ELO[2],rcur); }
  { int orung=c->school_rung+1; if(orung>=NRUNG)orung=NRUNG-1;
  fprintf(f,"  \"school\": {\"rung\": %d, \"rungs\": %d, \"elo\": %d, \"score\": %.3f, \"games\": %ld, \"observed\": %ld, \"obs_elo\": %d},\n",
    c->school_rung, NRUNG, RUNG_ELO[c->school_rung<0?0:(c->school_rung>=NRUNG?NRUNG-1:c->school_rung)], c->school_score, c->school_games, c->observed, RUNG_ELO[orung]); }
  fprintf(f,"  \"memory\": {\"concepts\": %d, \"core\": %d, \"imagined\": %d, \"deep_modules\": %d, \"wakes\": %ld, \"sleeps\": %ld, \"folders_atk\": %d, \"folders_def\": %d, \"deep\": %d, \"reflected\": %ld, \"recognized\": %ld, \"plan_follow\": %ld, \"plan_break\": %ld},\n",alive,core,imag,c->nloaded,c->wakes,c->sleeps,fatk,fdef,ndeep,DEEP_HARVEST,DEEP_HITS,PLAN_FOLLOW,PLAN_BREAK);
  fprintf(f,"  \"nucleo\": {\"merit_avg\": %.3f, \"merit_min\": %.3f, \"rated\": %d, \"per_situ\": %d, \"special\": %d},\n",cmavg,cmmin,cmn,NUC_PER_SITU,spec);
  {int ng_on=0; for(int i=0;i<NGENES;i++) if(GENES[i].on)ng_on++;
   fprintf(f,"  \"senses\": {\"genes\": %d, \"expressed\": %d, \"hot\": %d, \"pool\": %d, \"on_trial\": %s, \"trial_ends_in\": %ld},\n",
     NGENES,ng_on, NGENES, NGENE_TOTAL, c->trial_active?"true":"false", c->trial_active?(c->trial_until-c->age):0);}
  fprintf(f,"  \"study\": {\"pgn_games\": %ld, \"pgn_positions\": %ld},\n",c->pgn_games,c->pgn_pos);
  fprintf(f,"  \"genome\": \""); for(int i=0;i<c->glen;i++)fputc(c->genome[i],f); fprintf(f,"\",\n");
  fprintf(f,"  \"params\": {\"aprendizaje\":%.2f,\"umbral\":%.2f,\"retencion\":%.3f,\"profundidad\":%.1f,\"hambre\":%.2f,\"peso_energia\":%.2f,\"peso_placer\":%.2f,\"temperatura\":%.2f,\"mutabilidad\":%.2f,\"creatividad\":%.2f,\"gozo_comer\":%.2f,\"asco_comido\":%.2f,\"asco_vano\":%.2f,\"metabolismo\":%.2f,\"apetito_rey\":%.2f,\"valor_peon\":%.2f,\"valor_caballo\":%.2f,\"valor_alfil\":%.2f,\"valor_torre\":%.2f,\"valor_dama\":%.2f},\n",
    c->par[0],c->par[1],c->par[2],c->par[3],c->par[4],c->par[5],c->par[6],c->par[7],c->par[8],c->par[9],
    c->par[10],c->par[11],c->par[12],c->par[13],c->par[P_KHUNT],
    c->pval[1],c->pval[2],c->pval[3],c->pval[4],c->pval[5]);
  fprintf(f,"  \"game\": {\"active\":%s,\"user_side\":\"%s\",\"turn\":\"%s\",\"board\":\"%s\",\"last\":\"%s\",\"result\":%d,\"legal\":[",
    GAME_ACTIVE?"true":"false",USER_SIDE>0?"blancas":"negras",GAME.side>0?"blancas":"negras",bs,LASTMOVE,GAME_RESULT);
  if(GAME_ACTIVE&&GAME.side==USER_SIDE)for(int i=0;i<nm;i++){char a='a'+file_of(L[i].from),b='1'+rank_of(L[i].from),cc='a'+file_of(L[i].to),dd='1'+rank_of(L[i].to);fprintf(f,"%s\"%c%c%c%c\"",i?",":"",a,b,cc,dd);}
  fprintf(f,"]}\n}\n"); fclose(f); remove(p); rename(t,p); }

/* ============================ EVOLUCIÓN (μ+1 ligera) ====================== */
/* un duelo: cada bando juega con SU fenotipo (instintos + sentidos propios); comparten el mapa */
static int duel_game(Cell* c,const Pheno* phW,const Pheno* phB){
  Pos p; startpos(&p);
  double savedp[NPARAM]; memcpy(savedp,c->par,sizeof savedp);
  Gene savedg[GMAX]; int savedng=NGENES; memcpy(savedg,GENES,sizeof savedg); IN_DUEL=1;
  int sv=c->eval_mode; c->eval_mode=1; int res=0;
  for(int ply=0;ply<160;ply++){ Move L[256]; int n=gen_moves(&p,L);
    if(n==0){ res=in_check(&p,p.side)?(p.side>0?-1:1):0; break; }
    const Pheno* ph = p.side>0?phW:phB; memcpy(c->par,ph->par,sizeof(double)*NPARAM); apply_genes(ph);
    int t; Move m=choose(c,&p,0,&t); Pos nx; make_move(&p,&m,&nx); p=nx;
    if(ply==159){ int d=piece_count(&p,1)-piece_count(&p,-1); res=d>1?1:(d<-1?-1:0); break; } }
  c->eval_mode=sv; memcpy(c->par,savedp,sizeof savedp); memcpy(GENES,savedg,sizeof savedg); NGENES=savedng; IN_DUEL=0; GENOME_VER++; return res;
}
typedef struct { int result, ply, pw, pb; } DuelOut;
/* como duel_game pero devuelve también cuánto duró y con cuánto material terminó cada lado */
static int duel_ex(Cell* c,const Pheno* phW,const Pheno* phB,DuelOut* o){
  Pos p; startpos(&p);
  double savedp[NPARAM]; memcpy(savedp,c->par,sizeof savedp);
  Gene savedg[GMAX]; int savedng=NGENES; memcpy(savedg,GENES,sizeof savedg); IN_DUEL=1;
  int sv=c->eval_mode; c->eval_mode=1; int res=0,ply=0;
  for(ply=0;ply<160;ply++){ Move L[256]; int n=gen_moves(&p,L);
    if(n==0){ res=in_check(&p,p.side)?(p.side>0?-1:1):0; break; }
    const Pheno* ph=p.side>0?phW:phB; memcpy(c->par,ph->par,sizeof(double)*NPARAM); apply_genes(ph);
    int t; Move m=choose(c,&p,0,&t); Pos nx; make_move(&p,&m,&nx); p=nx;
    if(ply==159){ int d=piece_count(&p,1)-piece_count(&p,-1); res=d>1?1:(d<-1?-1:0); break; } }
  c->eval_mode=sv; memcpy(c->par,savedp,sizeof savedp); memcpy(GENES,savedg,sizeof savedg); NGENES=savedng; IN_DUEL=0; GENOME_VER++;
  if(o){ o->result=res; o->ply=ply; o->pw=piece_count(&p,1); o->pb=piece_count(&p,-1); }
  return res;
}
/* margen del DESAFIANTE juzgando su VERDADERA TAREA: ganar domina, pero también vale
   resistir más tiempo (aunque pierda) y conservar más material. Así no se filtra un sentido
   que no flipea el resultado pero sí alarga la partida o protege piezas. */
static double chal_margin(const DuelOut* o,int chal_white){
  double base = chal_white? o->result : -o->result;
  double mat  = chal_white? (o->pw-o->pb) : (o->pb-o->pw);
  double res  = (base<=0)? 0.40*(o->ply/160.0) : 0.0;     /* resistencia: durar cuando no ganás */
  return base + 0.30*(mat/8.0) + res;
}
/* COEVOLUCIÓN con genoma que CRECE: la mutación a veces duplica un sentido (y lo hace divergir),
   y el mutante pelea un match contra el campeón. Si gana claro, se adopta. El rival sube con él. */
/* afina un instinto (mutación puntual en los 30 genes de instinto), dueleada estricta */
static void tune_instincts(Cell* c){
  static char champG[GENOME_CAP]; int champLen=c->glen; memcpy(champG,c->genome,champLen+1);
  Pheno champ; decode_pheno(champG,champLen,&champ,NULL,NULL);
  int pos=rndi(GENE_OFF); char prev=c->genome[pos]; c->genome[pos]=bch(rndi(4)); decode_params(c);
  Pheno mut; decode_pheno(c->genome,c->glen,&mut,NULL,NULL);
  int score=0; for(int i=0;i<4;i++){ int r=(i%2==0)?duel_game(c,&mut,&champ):-duel_game(c,&champ,&mut); score+=r; }
  c->coevo_chal++; c->coevo_last=score;                                  /* un desafío al campeón */
  if(score>1){ c->coevo_win++; c->adoptions++; logevt("EVOLUCION","mejora instinto %+d (gen %ld)",score,c->generation); }
  else { c->genome[pos]=prev; decode_params(c); }
}
#define AUDIT_PRUNE -0.6   /* poda SOLO si el sentido molesta de verdad (score claramente negativo) */
#define AUDIT_SWAP   0.8   /* reemplaza SOLO si el candidato es claramente mejor que el titular */
#define BIRTH_GATE  -0.8   /* gate suave al nacer: veta el sentido nuevo SOLO si baja la vara claro */
#ifndef ENEMY_EYES_BIAS
#define ENEMY_EYES_BIAS 0.6   /* prob. de que un sentido NUEVO nazca mirando AL ENEMIGO (piezas del rival y/o referencia al rey rival). La población evolucionó casi ciega al rival (mira sus propias piezas); esto le da variantes para PERCIBIR DEBILIDADES ENEMIGAS. El gate de nacimiento descarta las que no aportan, así que NO fuerza nada: solo diversifica la mutación para sacarlo del pozo de "mirarse a sí mismo". 0 = comportamiento viejo. */
#endif
/* AUDITORÍA DE FONDO: agarra un sentido viejo al azar y lo juzga por su VERDADERA TAREA
   (resultado + resistencia + material). Dos modos, al azar:
   - PODA: compara el organismo CON vs SIN el sentido. Lo poda solo si ACTIVAMENTE MOLESTA
     (score < AUDIT_PRUNE). Un sentido neutro —o recién nacido que el mapa todavía no aprendió—
     puntúa ~0 y se queda: así los sentidos nuevos se acumulan en vez de borrarse apenas entran.
   - REEMPLAZO: desafía al sentido con una variante divergente; si es CLARAMENTE MEJOR, la adopta. */
static void audit_sense(Cell* c,int ngenes){
  int slot=rndi(ngenes), off=GENE_OFF+slot*GENE_LEN, curLen=c->glen;
  Pheno phCur; decode_pheno(c->genome,curLen,&phCur,NULL,NULL);
  DuelOut o;
  if(rnd()<0.55){                                   /* --- modo PODA: ¿este sentido molesta? --- */
    static char offG[GENOME_CAP]; int offLen=curLen-GENE_LEN; if(offLen<GENE_OFF)offLen=GENE_OFF;
    memcpy(offG,c->genome,off);                                        /* lo de antes del sentido */
    memcpy(&offG[off],&c->genome[off+GENE_LEN],curLen-(off+GENE_LEN));  /* colapsa el hueco */
    offG[offLen]=0;
    Pheno phOff; decode_pheno(offG,offLen,&phOff,NULL,NULL);
    double keep=0;                                   /* aporte del sentido = CON vs SIN */
    for(int i=0;i<6;i++){ if(i%2==0){ duel_ex(c,&phCur,&phOff,&o); keep+=chal_margin(&o,1); }
                          else        { duel_ex(c,&phOff,&phCur,&o); keep+=chal_margin(&o,0); } }
    if(keep<AUDIT_PRUNE){                             /* molesta de verdad -> poda */
      memcpy(&c->genome[off],&c->genome[off+GENE_LEN],curLen-(off+GENE_LEN));
      c->glen=offLen; c->genome[c->glen]=0; decode_params(c);
      logevt("EVOLUCION","sentido PODADO (no aportaba) %+.1f | %d sentidos (gen %ld)",keep,ngenes-1,c->generation); }
    /* si es neutro (no molesta), se queda en silencio: dejamos que se acumulen */
  } else {                                           /* --- modo REEMPLAZO: ¿hay una ruta mejor? --- */
    static char chalG[GENOME_CAP]; int chalLen=curLen; memcpy(chalG,c->genome,chalLen+1);
    chalG[off+rndi(GENE_LEN)]=bch(rndi(4)); chalG[off]=(rnd()<0.5?'G':'T');  /* candidato divergente, expresado */
    Pheno phChal; decode_pheno(chalG,chalLen,&phChal,NULL,NULL);
    double swap=0;                                   /* margen del candidato vs el titular */
    for(int i=0;i<6;i++){ if(i%2==0){ duel_ex(c,&phChal,&phCur,&o); swap+=chal_margin(&o,1); }
                          else        { duel_ex(c,&phCur,&phChal,&o); swap+=chal_margin(&o,0); } }
    c->coevo_chal++; c->coevo_last=swap;             /* otro desafío al campeón (el titular vigente) */
    if(swap>AUDIT_SWAP){                              /* claramente mejor -> reemplaza */
      memcpy(&c->genome[off],&chalG[off],GENE_LEN); decode_params(c); c->coevo_win++; c->adoptions++;
      logevt("EVOLUCION","sentido REEMPLAZADO por uno mejor %+.1f | %d sentidos (gen %ld)",swap,ngenes,c->generation); }
  }
}
/* EVOLUCIÓN — adopción instantánea + auditoría diferida:
   un sentido nuevo entra YA, expresado y usándose, sin período de prueba (aprende rápido).
   De fondo, una auditoría agarra sentidos viejos y poda los que molestan o los reemplaza por
   algo mejor; los neutros se quedan. La calidad se cuida después, sin frenar el aprendizaje. */
static void evolve(Cell* c){
  int ngenes=(c->glen-GENE_OFF)/GENE_LEN;
  /* ADOPCIÓN INSTANTÁNEA: nace un sentido nuevo (copia divergente) y entra al toque */
  if(ngenes<GPOOL && (ngenes==0 || rnd()<0.45)){       /* SIN TECHO de 256: el pool frío puede ser enorme */
    char block[GENE_LEN];
    if(ngenes>0) memcpy(block,&c->genome[GENE_OFF+rndi(ngenes)*GENE_LEN],GENE_LEN);
    else for(int k=0;k<GENE_LEN;k++)block[k]=bch(rndi(4));
    block[rndi(12)]=bch(rndi(4)); block[0]=(rnd()<0.5?'G':'T');   /* diverge en QUÉ/CUÁNDO (bases 0-11) y nace expresado */
    block[11]=bch(rndi(4));                                       /* elige su propio contexto (fase) */
    if(rnd()<ENEMY_EYES_BIAS){                                    /* OJOS HACIA EL ENEMIGO: que algunos nazcan mirando al rival */
      if(rnd()<0.6) block[3]='G';                                 /* base 3='G' -> de = piezas DEL RIVAL */
      if(rnd()<0.6) block[4]='C';                                 /* base 4='C' -> cerca de = REY RIVAL */
    }                                                             /* la mezcla da: piezas rivales junto al rey rival (¿qué tan defendido está?), mis piezas junto al rey rival (presión), etc. */
    { int npv=(int)((PRIO_BIRTH-1)*255.0/99.0+0.5);               /* PRIORIDAD de nacimiento alta: entra al caliente y el uso decide */
      block[12]=bch((npv>>6)&3); block[13]=bch((npv>>4)&3); block[14]=bch((npv>>2)&3); block[15]=bch(npv&3); }
    /* GATE SUAVE AL NACER: entra YA salvo que EMPEORE claro contra la mejor versión (el campeón).
       Rápido (4 duelos), no la prueba lenta de antes: solo veta a los que bajan la vara. Honra el
       "solo te quedás si no le bajás el nivel a la mejor versión de mí mismo", sin frenar el aprendizaje. */
    if(ngenes>0){
      static char champG[GENOME_CAP]; int champLen=c->glen; memcpy(champG,c->genome,champLen+1);    /* campeón = sin el sentido nuevo */
      Pheno phChamp; decode_pheno(champG,champLen,&phChamp,NULL,NULL);
      static char chalG[GENOME_CAP]; int chalLen=champLen; memcpy(chalG,champG,chalLen);
      for(int k=0;k<GENE_LEN;k++)chalG[chalLen++]=block[k]; chalG[chalLen]=0;            /* retador = con el sentido nuevo */
      Pheno phChal; decode_pheno(chalG,chalLen,&phChal,NULL,NULL);
      double s=0; DuelOut o;
      for(int i=0;i<4;i++){ if(i%2==0){ duel_ex(c,&phChal,&phChamp,&o); s+=chal_margin(&o,1); }
                            else        { duel_ex(c,&phChamp,&phChal,&o); s+=chal_margin(&o,0); } }
      c->coevo_chal++; c->coevo_last=s;                                                  /* un desafío al campeón */
      if(s<BIRTH_GATE){ c->coevo_veto++; c->generation++; measure(c);                    /* bajaba la vara -> ni nace */
        logevt("EVOLUCION","sentido nuevo VETADO al nacer %+.1f (le bajaba la vara al campeón) | %d sentidos (gen %ld)",s,ngenes,c->generation); return; }
      if(s>0) c->coevo_win++;                                                            /* además mejoró: cuenta como victoria */
    }
    for(int k=0;k<GENE_LEN;k++)c->genome[c->glen++]=block[k]; c->genome[c->glen]=0; decode_params(c);
    logevt("EVOLUCION","sentido nuevo ACEPTADO | %d sentidos (gen %ld)",ngenes+1,c->generation);
    c->generation++; measure(c); return;
  }
  /* AUDITORÍA DE FONDO: juzga / poda / reemplaza sentidos ya existentes */
  if(ngenes>0 && rnd()<0.50){ audit_sense(c,ngenes); c->generation++; measure(c); return; }
  /* y si no toca ni crecer ni auditar, afina los instintos */
  tune_instincts(c); c->generation++; measure(c);
}

/* ============================ AUTOPSIA ==================================== */
/* vuelca la memoria viva (módulos en disco) a un JSON legible, usando el propio
   conocimiento de la estructura del organismo (exacto en cualquier plataforma) */
static void autopsy_dump(Cell* c){
  static const char* PN[NPARAM]={"aprendizaje","umbral","retencion","profundidad","hambre","peso_energia","peso_placer","temperatura","mutabilidad","creatividad","gozo_comer","asco_comido","asco_vano","metabolismo","apetito_rey"};
  char path[1200]; snprintf(path,sizeof path,"%s/autopsia.json",c->nido);
  FILE* o=fopen(path,"w"); if(!o){ printf("no pude escribir %s\n",path); return; }
  fprintf(o,"{\n  \"generation\": %ld,\n  \"genome\": \"%s\",\n",c->generation,c->genome);
  fprintf(o,"  \"params\": {");
  for(int i=0;i<NPARAM;i++)fprintf(o,"%s\"%s\":%.4f",i?",":"",PN[i],c->par[i]);
  fprintf(o,"},\n  \"valores_aprendidos\": {\"peon\":%.3f,\"caballo\":%.3f,\"alfil\":%.3f,\"torre\":%.3f,\"dama\":%.3f},\n",
    c->pval[1],c->pval[2],c->pval[3],c->pval[4],c->pval[5]);
  fprintf(o,"  \"feature_legend\": {\"balance_material\":12,\"lado\":13,\"jaque\":23,\"movilidad\":22,\"escudo_rey_blanco\":42,\"escudo_rey_negro\":43,\"fase_piezas\":44},\n");
  { Pheno ph; decode_pheno(c->genome,c->glen,&ph,NULL,NULL);
    static const char* TIPO[7]={"cualquiera","peon","caballo","alfil","torre","dama","rey"};
    static const char* BANDO[3]={"mias","del_rival","cualquiera"};
    static const char* REF[4]={"mi_rey","rey_rival","centro","todo_el_tablero"};
    static const char* COND[NCOND]={"mi_rey_en_jaque","rey_rival_en_jaque","dama_rival_viva","mi_dama_viva",
      "es_final","es_apertura","ventaja_material","desventaja_material","juego_blancas","mi_rey_expuesto",
      "dama_rival_cerca_de_mi_rey","mi_dama_cerca_del_rey_rival"};
    fprintf(o,"  \"senses\": [\n");
    for(int i=0;i<ph.ng;i++){ Gene* g=&ph.gene[i];
      fprintf(o,"    {\"gen\":%d,\"expresado\":%s,\"cuenta\":\"%s\",\"de\":\"%s\",\"cerca_de\":\"%s\",\"radio\":%d,\"despierta_cuando\":[",
        i, g->on?"true":"false", TIPO[g->type], BANDO[g->side], REF[g->ref], g->radius);
      int first=1; for(int k=0;k<NCOND;k++) if(g->trig&(1<<k)){ fprintf(o,"%s\"%s\"",first?"":",",COND[k]); first=0; }
      fprintf(o,"]}%s\n", i<ph.ng-1?",":""); }
    fprintf(o,"  ],\n"); }
  fprintf(o,"  \"concepts\": [\n");
  int first=1,total=0,core=0,imag=0,deep=0;
  for(int k=0;k<MODKEY_MAX;k++){ char mp[1200]; mod_path(k,mp,sizeof mp); FILE* f=fopen(mp,"rb"); if(!f)continue;
    int ver=0,cnt=0; if(fread(&ver,4,1,f)!=1||ver!=MEM_VER){fclose(f);continue;} if(fread(&cnt,4,1,f)!=1){fclose(f);continue;}
    for(int i=0;i<cnt;i++){ Concept cc; if(fread(&cc,sizeof(Concept),1,f)!=1)break; if(!cc.alive)continue;
      total++; if(cc.core)core++; if(cc.born_dream)imag++; if(cc.deep)deep++;
      if(!first)fprintf(o,",\n"); first=0;
      fprintf(o,"    {\"gid\":%ld,\"mod\":%d,\"core\":%d,\"imaginada\":%d,\"nivel\":%.2f,\"profundo\":%d,\"jugada\":\"%d-%d\",\"consecuencia\":%.1f,\"v_e\":%.4f,\"v_p\":%.4f,\"strength\":%.3f,\"useful\":%.3f,\"uses\":%ld,\"deg\":%d,\"feat\":[",
        cc.gid,cc.module_key,cc.core,cc.born_dream,cc.level,cc.deep,cc.dfrom,cc.dto,cc.dscore,cc.v_e,cc.v_p,cc.strength,cc.useful,cc.uses,cc.deg);
      for(int d=0;d<FEATD;d++)fprintf(o,"%s%.3f",d?",":"",cc.feat[d]); fprintf(o,"]}");
    } fclose(f);
  }
  fprintf(o,"\n  ],\n  \"totals\": {\"concepts\": %d, \"core\": %d, \"imaginadas\": %d, \"profundos\": %d}\n}\n",total,core,imag,deep);
  fclose(o);
  printf("autopsia escrita en %s  (%d conceptos, %d en nucleo, gen %ld)\n",path,total,core,c->generation);
}

/* ================================== MAIN ================================== */
int main(int argc,char** argv){ const char* nido="./nido"; long maxticks=0; int do_autopsia=0;
  for(int i=1;i<argc;i++){ if(!strcmp(argv[i],"--nido")&&i+1<argc)nido=argv[++i]; else if(!strcmp(argv[i],"--ticks")&&i+1<argc)maxticks=atol(argv[++i]); else if(!strcmp(argv[i],"--autopsia"))do_autopsia=1; }
  srand((unsigned)time(NULL));
  Cell* c=calloc(1,sizeof(Cell)); snprintf(c->nido,sizeof c->nido,"%s",nido); snprintf(REAL_NIDO,sizeof REAL_NIDO,"%s",nido);
  c->last_active=-1; c->energy=10; c->placer=0; c->e_target=12; GC=c;
  for(int t=1;t<=5;t++) c->pval[t]=0.3;   /* valores de pieza arrancan PAREJOS: la observación de la máquina los diferencia */
  MKDIR(nido); {char m[1300];snprintf(m,sizeof m,"%s/memoria",nido);MKDIR(m);} {char m[1300];snprintf(m,sizeof m,"%s/maestros",nido);MKDIR(m);}
  version_guard(c);   /* si el nido es de una versión vieja del Concept, lo limpia ANTES de cargar nada (evita correr roto) */
  snprintf(BITPATH,sizeof BITPATH,"%s/bitacora.log",nido); BIT=fopen(BITPATH,"a");
  if(!load_genome(c)){ seed_genome(c); save_genome(c); logevt("GENESIS","el organismo ajedrecista nace sin saber nada del ajedrez"); }
  else logevt("DESPIERTA","retoma su ADN y su memoria");
  load_presets(c);   /* caché de selección de sentidos: arranca caliente si quedó de antes */
  {char p[1200];snprintf(p,sizeof p,"%s/memoria/_index.dat",nido);FILE* f=fopen(p,"rb");if(f){if(fread(&c->next_gid,sizeof c->next_gid,1,f)!=1)c->next_gid=0;fclose(f);}}
  load_stats(c); load_vhead(c); load_nucleo(c);   /* el núcleo vive aparte y se carga residente desde el arranque */
  if(do_autopsia){ autopsy_dump(c); if(BIT)fclose(BIT); free(c); return 0; }
  study_all_masters(c);   /* estudio inicial de maestros (dato real: siembra el núcleo con ajedrez de verdad) */
  measure(c); write_snapshot(c);
  long t=0;
  while(maxticks==0||t<maxticks){ c->age=t;
    poll_command(c);
    GTRACK=1;
    if((c->dreams & 1) || t < SELFPLAY_FROM){                   /* impares observan maestros; ADEMÁS, antes del tick SELFPLAY_FROM TODO es maestros (fundación, sin jugar todavía) */
      CUR_REALITY=0.95f;                                        /* mirar maestros = experiencia muy real */
      study_pgn_drip(c, GAMES_PER_DRIP);                        /* observa GAMES_PER_DRIP partidas: percibe, refuerza conceptos y entrena V */
    } else {                                                    /* la otra: JUEGA contra su nivel y el resultado mueve la vara (la escuelita sigue viva) */
      int bs=((c->dreams>>1)&1)?1:-1;                           /* alterna colores entre partidas jugadas */
      CUR_REALITY=0.30f+c->school_rung*0.10f;                   /* jugar un rival real = experiencia real (sube con el nivel) */
      int r=play_and_learn_ex(c,1,c->school_rung,bs,0);
      school_step(c, r==0?0.5:(r==bs?1.0:0.0));
    }
    GTRACK=0; c->dreams++;
    if(t%50==0) adjust_gene_priorities(c);   /* el uso ajusta la prioridad: los útiles suben, los ociosos se duermen */
    if(t%100==0) measure(c);
    if(t>0 && t%400==0) measure_sparring(c);   /* fuerza real contra la vara fija externa */
    if(t%80==0) evolve(c);
    if(t>0 && t%50==0) compete_nucleo(c);   /* compite, demota lo inferior/podrido y aplica el TOPE DURO del núcleo */
    if(t%5==0)  write_snapshot(c);
    if(t%200==0){ save_all_memory(c); save_genome(c); log_cap(); }
    if(t%5000==0) save_presets(c);
    t++;
  }
  save_all_memory(c); save_genome(c); save_presets(c); write_snapshot(c);
  if(BIT)fclose(BIT); free(c); return 0;
}
