/* =============================================================================
 *  CÉLULA DIGITAL — motor (Fase 3, organismo INMORTAL)
 *
 *  El organismo nunca muere. Es uno solo, continuo: su memoria (mapa conceptual)
 *  persiste para siempre. Mejora en tiempo de ejecución y evoluciona en TODOS
 *  los aspectos vía una estrategia evolutiva (μ+λ) de la que adopta solo mejoras
 *  (elitismo => nunca retrocede). El efecto Baldwin emerge: las variantes cuyo
 *  genoma aprende y PREDICE mejor son las que se seleccionan.
 *
 *  Pipeline por tick: predecir -> percibir datos -> aprender (mapa conceptual) ->
 *      expresar genes -> actuar (cuerpo, regulación) -> metabolizar (sin muerte)
 *  En paralelo: búsqueda evolutiva (población de genomas) -> adopción de mejoras.
 *
 *  Compilar:  gcc -O2 -o celula celula.c -lm
 *  Correr:    ./celula --nido ./nido --ticks 0        (vive para siempre)
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

/* mkdir portable: en Windows toma 1 argumento; en POSIX toma ruta + permisos */
#ifdef _WIN32
  #include <direct.h>
  #define MKDIR(p) _mkdir(p)
#else
  #define MKDIR(p) mkdir((p),0755)
#endif

/* ----------------------------- parámetros ---------------------------------- */
#define GRID        40
#define NREG        8
#define NSIG        8
#define STACKMAX    128
#define FEATD       48          /* percepción fina: histograma + n-gramas + autocorrelación + ventanas */
#define MAXCONCEPTS 256
#define MAXEDGES    8
#define CHUNK       4096
#define GENOME_MAX  4096
#define NPARAM      10          /* parámetros evolucionables (incluye creatividad y sueño) */
#define NCACHE      64          /* cache de rasgos para evaluar variantes */
#define MOD_VERSION FEATD       /* versión de formato de módulo = dimensión de rasgos */

/* población evolutiva (μ+λ)-ES */
#define MU          6
#define LAMBDA      10
#define EVAL_TICKS  300
#define GEN_EVERY   140         /* una generación evolutiva cada N ticks reales */

/* memoria jerárquica: rápida (RAM) + profunda (disco, en módulos independientes) */
#define MAXLOADED   6           /* cuántos módulos pueden estar despiertos en RAM a la vez */
#define MODKEY_MAX  256         /* cantidad de sectores/módulos posibles de la memoria profunda */
#define MEM_FLUSH   250         /* cada N ticks persiste a disco los módulos sucios */

/* --------------------------- código genético ------------------------------- */
enum { A=0, C=1, G=2, T=3 };
#define START_CODON 14
static const int STOPS[3] = {48,50,56};
enum {
  OP_NOP=0, OP_LIT, OP_DUP, OP_DROP, OP_SWAP, OP_ADD, OP_SUB, OP_CMP, OP_RAND,
  OP_ABSORB, OP_GROW, OP_EMIT, OP_SENSE,
  OP_TASTE, OP_LEARN, OP_RECALL,
  OP_BIND, OP_REPRESS, OP_READ, OP_EDIT, OP_DIVIDE, OP_APOPT, OP_WAIT,
  NUM_OPS
};
static int CODON2OP[64]; static int OP2CODON[NUM_OPS];
static int is_stop(int cd){ return cd==STOPS[0]||cd==STOPS[1]||cd==STOPS[2]; }
static void build_codon_table(void){
  for(int i=0;i<64;i++) CODON2OP[i]=-1;
  int op=0; for(int cd=0;cd<64;cd++){ if(cd==START_CODON||is_stop(cd))continue; CODON2OP[cd]=op%NUM_OPS; op++; }
  for(int k=0;k<NUM_OPS;k++) OP2CODON[k]=-1;
  for(int cd=0;cd<64;cd++) if(CODON2OP[cd]>=0 && OP2CODON[CODON2OP[cd]]<0) OP2CODON[CODON2OP[cd]]=cd;
}
static char base_char(int b){ return "ACGT"[b&3]; }
static int  char_base(char c){ switch(c){case 'A':return 0;case 'C':return 1;case 'G':return 2;case 'T':return 3;} return -1; }

/* parámetros decodificados desde el ADN */
enum { P_LEARN, P_JOIN, P_DECAY, P_GROW, P_METAB, P_NOVELTY, P_PREDICT, P_MUT, P_CREAT, P_DREAM };

/* ------------------------------ mapa conceptual ----------------------------- */
typedef struct {
  long gid; int module_key;     /* identidad global estable + a qué módulo profundo pertenece */
  float feat[FEATD]; float strength; float useful; long uses; long last_age;
  int core, deg; long edge_to[MAXEDGES]; float edge_w[MAXEDGES]; int alive;  /* edges por gid */
  int born_dream;               /* 1 si fue creado por imaginación (idea), aún no validado por datos reales */
} Concept;

/* ------------------------------ la célula ---------------------------------- */
typedef struct {
  char genome[GENOME_MAX]; int glen;
  double par[NPARAM];
  double energy; long age; long generation; long lineage_id;
  int stack[STACKMAX]; int sp;
  double regulators[NREG]; double signals[NSIG];
  unsigned char body[GRID][GRID]; int body_count;
  Concept mem[MAXCONCEPTS]; int nconcepts; int last_active;
  double novelty, surprise, pred_acc;
  float last_feat[FEATD];
  int eval_mode;       /* 1 = variante en evaluación (usa cache, no disco) */
  int stream_file; long stream_off, stream_pos;  /* lectura secuencial = estructura temporal */
  long adoptions;      /* cuántas mejoras evolutivas adoptó el organismo */
  /* memoria jerárquica */
  long next_gid;                         /* próximo id global de concepto */
  int loaded_key[MAXLOADED]; long loaded_lru[MAXLOADED]; int loaded_dirty[MAXLOADED]; int nloaded;
  long wakes, sleeps;                    /* estadística de despertar/dormir módulos */
  /* creatividad / imaginación */
  int dreaming, last_created;            /* en sueño ; el último learn creó un concepto nuevo */
  long creations, creative_concepts, creative_hits, compositions, consolidations;
} Cell;

/* --------------------------- log + bitácora -------------------------------- */
#define LOG_N 50
typedef struct { long t; char tag[16]; char msg[160]; } LogRow;
static LogRow LOG[LOG_N]; static int log_head=0, log_count=0;
static FILE* BITACORA=NULL;
static void logevt(Cell* c, const char* tag, const char* fmt, ...){
  if(c->eval_mode) return;                 /* las variantes no ensucian la bitácora */
  char msg[160]; va_list ap; va_start(ap,fmt); vsnprintf(msg,sizeof msg,fmt,ap); va_end(ap);
  LogRow* r=&LOG[log_head]; r->t=c->age; snprintf(r->tag,sizeof r->tag,"%s",tag);
  snprintf(r->msg,sizeof r->msg,"%s",msg);
  log_head=(log_head+1)%LOG_N; if(log_count<LOG_N) log_count++;
  if(BITACORA){ time_t now=time(NULL); char ts[32]; struct tm* tm=localtime(&now);
    strftime(ts,sizeof ts,"%Y-%m-%d %H:%M:%S",tm);
    fprintf(BITACORA,"%s\tt=%ld\tgen=%ld\t[%s]\t%s\n",ts,c->age,c->generation,tag,msg); fflush(BITACORA); }
}

/* ------------------------------ utilidades --------------------------------- */
static double rnd(void){ return (double)rand()/((double)RAND_MAX+1.0); }
static int    rndi(int n){ return n>0?(int)(rnd()*n):0; }
static double clampd(double x,double a,double b){ return x<a?a:(x>b?b:x); }

/* ============================================================================
 *  GENOMA
 * ========================================================================== */
static int codon_at(Cell* c,int i){
  if(i+2>=c->glen) return -1;
  int b0=char_base(c->genome[i]),b1=char_base(c->genome[i+1]),b2=char_base(c->genome[i+2]);
  if(b0<0||b1<0||b2<0) return -1; return b0*16+b1*4+b2;
}
static void decode_params(Cell* c){
  /* primeros NPARAM codones = región reguladora -> parámetros (todos evolucionables) */
  static const double LO[NPARAM]={0.03,0.08,0.990,0.0,0.5,0.0,0.0,0.05,0.0,0.0};
  static const double HI[NPARAM]={0.50,0.30,0.9999,1.0,1.6,1.0,2.0,0.60,1.0,0.6};
  for(int i=0;i<NPARAM;i++){ int cd=codon_at(c,i*3); if(cd<0)cd=31;
    c->par[i]=LO[i]+(cd/63.0)*(HI[i]-LO[i]); }
}
static void g_base(Cell* c,int b){ if(c->glen<GENOME_MAX-1) c->genome[c->glen++]=base_char(b); }
static void g_codon(Cell* c,int cd){ g_base(c,(cd>>4)&3); g_base(c,(cd>>2)&3); g_base(c,cd&3); }
static void g_op(Cell* c,int op){ g_codon(c,OP2CODON[op]); }
static void g_gene(Cell* c,int reg,int act,int um,const int* ops,int n){
  g_base(c,reg&3); g_base(c,act?G:C); g_base(c,um&3); g_codon(c,START_CODON);
  for(int i=0;i<n;i++){ if(ops[i]==OP_LIT){ g_op(c,OP_LIT); g_codon(c,ops[++i]&63);} else g_op(c,ops[i]); }
  g_codon(c,STOPS[0]);
}
static void seed_genome(Cell* c){
  c->glen=0;
  for(int i=0;i<NPARAM;i++) g_codon(c,31);          /* región reguladora (params medios) */
  for(int i=0;i<6;i++) g_base(c,rndi(4));
  { int ops[]={OP_ABSORB,OP_TASTE,OP_LEARN,OP_RECALL}; g_gene(c,0,1,0,ops,4); } /* metabolizar+aprender+predecir */
  for(int i=0;i<6;i++) g_base(c,rndi(4));
  { int ops[]={OP_LIT,1,OP_GROW}; g_gene(c,3,1,1,ops,3); }                       /* crecer si "cómodo" (familiar) */
  for(int i=0;i<6;i++) g_base(c,rndi(4));
  { int ops[]={OP_LIT,6,OP_EMIT,OP_TASTE}; g_gene(c,6,1,1,ops,4); }              /* explorar si hay novedad */
  for(int i=0;i<6;i++) g_base(c,rndi(4));
  c->genome[c->glen]=0; decode_params(c);
}
static int load_genome(Cell* c,const char* path){
  FILE* f=fopen(path,"r"); if(!f) return 0; c->glen=0; int ch;
  while((ch=fgetc(f))!=EOF && c->glen<GENOME_MAX-1){
    if(ch=='#'||ch=='>'){ while((ch=fgetc(f))!=EOF&&ch!='\n'); continue; }
    if(ch=='A'||ch=='C'||ch=='G'||ch=='T') c->genome[c->glen++]=(char)ch;
  }
  c->genome[c->glen]=0; fclose(f); decode_params(c); return c->glen>0;
}
static void save_genome(Cell* c,const char* path){
  FILE* f=fopen(path,"w"); if(!f) return;
  fprintf(f,"# celula digital INMORTAL — genoma (gen %ld, adopciones %ld)\n>chr1\n",c->generation,c->adoptions);
  for(int i=0;i<c->glen;i++){ fputc(c->genome[i],f); if((i+1)%60==0) fputc('\n',f); }
  fputc('\n',f); fclose(f);
}

/* ============================================================================
 *  RASGOS + cache de datos
 * ========================================================================== */
static float FCACHE[NCACHE][FEATD]; static int FCN=0;
static void features(const unsigned char* buf,int n,float out[FEATD]){
  for(int i=0;i<FEATD;i++) out[i]=0; if(n<=0) return;
  long h16[16]={0}; double mean=0; long printable=0, runs=0; int prev=-1;
  unsigned char seen[256]={0}; long distinct=0;
  for(int i=0;i<n;i++){ int b=buf[i]; h16[b>>4]++; mean+=b; if(b>=32&&b<127)printable++;
    if(b==prev)runs++; prev=b; if(!seen[b]){seen[b]=1;distinct++;} }
  for(int k=0;k<16;k++) out[k]=(float)h16[k]/n;          /* [0..15] histograma fino */
  mean/=n;
  /* [16..23] bigramas hasheados (patrones locales de pares) */
  long bg[8]={0}, nb=0;
  for(int i=0;i+1<n;i++){ int hsh=((buf[i]*131+buf[i+1])&0xff)%8; bg[hsh]++; nb++; }
  if(nb>0) for(int k=0;k<8;k++) out[16+k]=(float)bg[k]/nb;
  /* [24..28] autocorrelación a lags 1,2,4,8,16 (periodicidad/repetición multiescala) */
  int lags[5]={1,2,4,8,16};
  for(int li=0;li<5;li++){ int L=lags[li]; long s=0,cnt=0;
    for(int i=0;i+L<n;i++){ if(buf[i]==buf[i+L]) s++; cnt++; } out[24+li]=cnt?(float)s/cnt:0; }
  /* [29..36] media y entropía por 4 ventanas (cómo cambia la textura) */
  for(int w=0;w<4;w++){ int a=w*n/4, e=(w+1)*n/4; long wc=e-a; double wm=0; long wh[8]={0};
    for(int i=a;i<e;i++){ wm+=buf[i]; wh[buf[i]>>5]++; }
    out[29+w]=wc?(float)((wm/wc)/255.0):0;
    double we=0; for(int k=0;k<8;k++){ double p=wc?(double)wh[k]/wc:0; if(p>0)we-=p*log2(p);} out[33+w]=(float)(we/3.0); }
  /* [37] entropía global, [38] texto, [39] repetición, [40] diversidad de bytes */
  long h8[8]={0}; for(int i=0;i<n;i++) h8[buf[i]>>5]++;
  double ent=0; for(int k=0;k<8;k++){ double p=(double)h8[k]/n; if(p>0)ent-=p*log2(p);} out[37]=(float)(ent/3.0);
  out[38]=(float)printable/n; out[39]=(float)runs/n; out[40]=(float)distinct/256.0;
  /* [41] media, [42] desvío */
  out[41]=(float)(mean/255.0);
  double var=0; for(int i=0;i<n;i++){double d=buf[i]-mean; var+=d*d;} var/=n; out[42]=(float)(sqrt(var)/128.0);
  /* [43] tamaño, [44] primer byte, [45] último byte */
  out[43]=(float)clampd(n/(double)CHUNK,0,1); out[44]=(float)(buf[0]/255.0); out[45]=(float)(buf[n-1]/255.0);
  /* [46] variedad de bigramas, [47] racha más larga */
  int nzb=0; for(int k=0;k<8;k++) if(bg[k]>0)nzb++; out[46]=(float)nzb/8.0;
  long lr=0,cur=1; for(int i=1;i<n;i++){ if(buf[i]==buf[i-1])cur++; else cur=1; if(cur>lr)lr=cur; } out[47]=(float)lr/n;
}
static float feat_dist(const float* a,const float* b){ float s=0; for(int i=0;i<FEATD;i++){float d=a[i]-b[i]; s+=d*d;} return sqrtf(s); }
/* cache = stream ORDENADO de trozos consecutivos -> hay transiciones que aprender */
static void refresh_cache(const char* nido){
  char dir[1100]; snprintf(dir,sizeof dir,"%s/comedero",nido);
  FCN=0; DIR* d=opendir(dir);
  if(d){ struct dirent* e; char files[64][300]; int nf=0;
    while((e=readdir(d))&&nf<64){ if(e->d_name[0]=='.')continue; snprintf(files[nf],300,"%s/%s",dir,e->d_name); nf++; }
    closedir(d);
    for(int fi=0; fi<nf && FCN<NCACHE; fi++){
      FILE* f=fopen(files[fi],"rb"); if(!f)continue;
      for(int ck=0; ck<8 && FCN<NCACHE; ck++){
        unsigned char buf[CHUNK]; int n=(int)fread(buf,1,CHUNK,f); if(n<=0) break;
        features(buf,n,FCACHE[FCN]); FCN++; if(n<CHUNK) break;
      }
      fclose(f);
    }
  }
  if(FCN==0){ for(int p=0;p<6;p++){ for(int i=0;i<FEATD;i++) FCACHE[p][i]=(float)((p*7+i*13)%9)/9.0f; FCN++; } }
}

/* ============================================================================
 *  APRENDIZAJE: mapa conceptual
 * ========================================================================== */
static int nearest_concept(Cell* c,const float* x,float* od){
  int best=-1; float bd=1e9f;
  for(int i=0;i<c->nconcepts;i++){ if(!c->mem[i].alive)continue; float d=feat_dist(c->mem[i].feat,x); if(d<bd){bd=d;best=i;} }
  if(od) *od=bd; return best;
}
static int alloc_slot(Cell* c){
  if(c->nconcepts<MAXCONCEPTS) return c->nconcepts++;
  for(int i=0;i<c->nconcepts;i++) if(!c->mem[i].alive) return i;   /* reutiliza slot podado */
  return -1;
}
static int find_by_gid(Cell* c,long gid){
  for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive && c->mem[i].gid==gid) return i;
  return -1;
}
static void add_edge(Cell* c,int a,int b){
  if(a<0||b<0||a==b) return; Concept* n=&c->mem[a]; long bg=c->mem[b].gid;  /* enlaza por id global */
  for(int e=0;e<n->deg;e++) if(n->edge_to[e]==bg){ n->edge_w[e]+=0.2f; return; }
  if(n->deg<MAXEDGES){ n->edge_to[n->deg]=bg; n->edge_w[n->deg]=0.4f; n->deg++; }
}

/* ============================================================================
 *  MEMORIA JERÁRQUICA: rápida (RAM) <-> profunda (disco, módulos independientes)
 * ========================================================================== */
static char REAL_NIDO[1024]="./nido";  /* nido del organismo real (las variantes no tocan disco) */
static int module_key(const float* x){              /* a qué sector pertenece un patrón (0..255) */
  int ent=(int)(clampd(x[37],0,0.999)*4);   /* entropía global */
  int pr =(int)(clampd(x[38],0,0.999)*4);   /* texto vs binario */
  int ac =(int)(clampd(x[24],0,0.999)*4);   /* repetición (autocorr lag 1) */
  int mn =(int)(clampd(x[41],0,0.999)*4);   /* media */
  return ent*64+pr*16+ac*4+mn;              /* 0..255 */
}
static void mod_path(Cell* c,int key,char* out,int n){ (void)c;
  snprintf(out,n,"%s/memoria/mod_%03d.dat",REAL_NIDO,key); }
static int loaded_idx(Cell* c,int key){ for(int i=0;i<c->nloaded;i++) if(c->loaded_key[i]==key) return i; return -1; }

/* escribe a disco todos los conceptos de un módulo; si evict, los saca de la RAM */
static void flush_module(Cell* c,int key,int evict){
  char p[1200]; mod_path(c,key,p,sizeof p);
  int cnt=0; for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive && c->mem[i].module_key==key) cnt++;
  FILE* f=fopen(p,"wb"); if(!f) return;
  int ver=MOD_VERSION; fwrite(&ver,sizeof ver,1,f);     /* versión de formato (= FEATD) */
  fwrite(&cnt,sizeof cnt,1,f);
  for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive && c->mem[i].module_key==key){
    fwrite(&c->mem[i],sizeof(Concept),1,f); if(evict) c->mem[i].alive=0; }
  fclose(f);
}
/* sube un módulo del disco a la RAM (lo "despierta"); ignora módulos de versión incompatible */
static int load_module(Cell* c,int key){
  char p[1200]; mod_path(c,key,p,sizeof p);
  FILE* f=fopen(p,"rb"); if(!f) return 0;
  int ver=0; if(fread(&ver,sizeof ver,1,f)!=1 || ver!=MOD_VERSION){ fclose(f); return 0; } /* memoria vieja => se ignora */
  int cnt=0; if(fread(&cnt,sizeof cnt,1,f)!=1){ fclose(f); return 0; }
  int loaded=0;
  for(int k=0;k<cnt;k++){ Concept tmp; if(fread(&tmp,sizeof(Concept),1,f)!=1) break;
    int s=alloc_slot(c); if(s<0) break; c->mem[s]=tmp; c->mem[s].alive=1; loaded++;
    if(tmp.gid>=c->next_gid) c->next_gid=tmp.gid+1; }
  fclose(f); return loaded;
}
static void evict_lru(Cell* c){
  if(c->nloaded==0) return; int idx=0; long best=c->loaded_lru[0];
  for(int i=1;i<c->nloaded;i++) if(c->loaded_lru[i]<best){ best=c->loaded_lru[i]; idx=i; }
  int key=c->loaded_key[idx];
  flush_module(c,key,1);                 /* lo duerme: persiste y libera la RAM */
  c->sleeps++; logevt(c,"DORMIR","módulo %d vuelve al disco (libera RAM)",key);
  for(int i=idx;i<c->nloaded-1;i++){ c->loaded_key[i]=c->loaded_key[i+1];
    c->loaded_lru[i]=c->loaded_lru[i+1]; c->loaded_dirty[i]=c->loaded_dirty[i+1]; }
  c->nloaded--;
}
/* garantiza que el sector del estímulo esté despierto en RAM */
static void ensure_loaded(Cell* c,int key){
  if(c->eval_mode) return;               /* las variantes en evaluación no tocan disco */
  int i=loaded_idx(c,key);
  if(i>=0){ c->loaded_lru[i]=c->age; return; }
  if(c->nloaded>=MAXLOADED) evict_lru(c);
  int got=load_module(c,key);
  c->loaded_key[c->nloaded]=key; c->loaded_lru[c->nloaded]=c->age; c->loaded_dirty[c->nloaded]=0; c->nloaded++;
  c->wakes++;
  if(got>0) logevt(c,"DESPIERTA","módulo %d sube a RAM (%d conceptos)",key,got);
}
static void mark_dirty(Cell* c,int key){ int i=loaded_idx(c,key); if(i>=0) c->loaded_dirty[i]=1; }
static void flush_all(Cell* c){          /* persiste todos los módulos sucios (sin desalojar) */
  if(c->eval_mode) return;
  for(int i=0;i<c->nloaded;i++) if(c->loaded_dirty[i]){ flush_module(c,c->loaded_key[i],0); c->loaded_dirty[i]=0; }
}

static double learn(Cell* c,const float* x,double gain){
  int key=module_key(x);
  ensure_loaded(c,key);                  /* despierta el sector profundo correspondiente */
  float d; int near=nearest_concept(c,x,&d);
  c->novelty=(near<0)?1.0:clampd(d,0,1);
  double fam=1.0/(1.0+(near<0?2.0:d));
  int cur; c->last_created=0;
  double jt = c->par[P_JOIN] * (c->dreaming ? 3.0 : 1.0);   /* soñando: más consolidar, menos inventar */
  if(near>=0 && d<jt){
    Concept* n=&c->mem[near]; float lr=(float)c->par[P_LEARN];
    for(int i=0;i<FEATD;i++) n->feat[i]+=lr*(x[i]-n->feat[i]);
    n->strength=(float)clampd(n->strength+0.25,0,10); n->uses++; n->last_age=c->age;
    n->useful=(float)clampd(n->useful*0.98+gain*0.1,0,10); cur=near;
    mark_dirty(c,n->module_key);
    if(c->dreaming && !n->born_dream){ c->consolidations++;           /* rehearsal: protege la memoria */
      n->strength=(float)clampd(n->strength+0.15,0,10); }
    /* FEEDBACK: una idea imaginada que los datos REALES confirman => creatividad útil */
    if(!c->dreaming && n->born_dream){ n->born_dream=0; c->creative_hits++;
      n->useful=(float)clampd(n->useful+0.3,0,10);
      logevt(c,"IDEA-OK","la idea gid%ld se confirmó con datos reales (creatividad útil)",n->gid); }
  } else {
    int slot=alloc_slot(c);
    if(slot>=0){
      Concept* n=&c->mem[slot]; memset(n,0,sizeof(Concept));
      memcpy(n->feat,x,sizeof(float)*FEATD);
      n->gid=c->next_gid++; n->module_key=key;
      n->strength=0.6f; n->useful=0.1f; n->uses=1; n->last_age=c->age; n->core=0; n->deg=0; n->alive=1;
      n->born_dream = c->dreaming ? 1 : 0;
      cur=slot; c->last_created=1; mark_dirty(c,key);
      if(c->dreaming) logevt(c,"IDEA","imaginó una idea nueva gid%ld en módulo %d",n->gid,key);
      else logevt(c,"CONCEPTO","nueva noción atómica gid%ld en módulo %d (novedad %.2f)",n->gid,key,c->novelty);
    } else cur=near;
  }
  add_edge(c,c->last_active,cur); add_edge(c,cur,c->last_active); c->last_active=cur;
  Concept* n=&c->mem[cur];
  if(!n->core && !n->born_dream && n->strength>3.5f && n->deg>=2 && n->useful>0.6f){ n->core=1; mark_dirty(c,n->module_key);
    logevt(c,"NUCLEO","concepto gid%ld ascendió a importante (resguardado)",n->gid); }
  double dec=c->par[P_DECAY];
  for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive && !c->mem[i].core && i!=cur) c->mem[i].strength*=(float)dec;
  return fam;
}

/* ============================================================================
 *  IMAGINACIÓN: recombina lo aprendido para crear ideas nuevas, y aprende de ellas
 *  (consolidación tipo "sueño"; la creatividad útil se premia, la inútil se poda)
 * ========================================================================== */
static void imagine(Cell* c){
  if(c->nconcepts<2) return;
  /* concepto semilla: el más fuerte de una muestra */
  int a=-1; float bestS=-1;
  for(int t=0;t<8;t++){ int i=rndi(c->nconcepts); if(c->mem[i].alive && c->mem[i].strength>bestS){ bestS=c->mem[i].strength; a=i; } }
  if(a<0) return;
  /* socio: el más asociado, o uno al azar -> recombinación de dos ideas conocidas */
  int b=-1; float bw=-1; Concept* na=&c->mem[a];
  for(int e=0;e<na->deg;e++){ int s=find_by_gid(c,na->edge_to[e]); if(s>=0 && c->mem[s].alive && na->edge_w[e]>bw){ bw=na->edge_w[e]; b=s; } }
  if(b<0){ for(int t=0;t<8;t++){ int i=rndi(c->nconcepts); if(c->mem[i].alive && i!=a){ b=i; break; } } }
  float created[FEATD]; float mix=0.5f+((float)rnd()-0.5f)*0.6f; float noise=(float)c->par[P_CREAT];
  for(int i=0;i<FEATD;i++){
    float base = (b>=0)? mix*c->mem[a].feat[i]+(1-mix)*c->mem[b].feat[i] : c->mem[a].feat[i];
    base += (float)((rnd()-0.5)*0.15*noise);
    created[i]=(float)clampd(base,0,1);
  }
  int prev=c->last_active; c->last_active=-1;             /* aísla el sueño de la cadena real */
  c->dreaming=1; learn(c,created,0.04); c->dreaming=0;    /* aprende de lo imaginado */
  c->last_active=prev;
  c->creations++;
  if(c->last_created) c->creative_concepts++;            /* nació una idea genuinamente nueva */
}

/* sintetiza bytes a partir del histograma fino de un concepto */
static int synth_bytes(const Concept* n,unsigned char* out,int len){
  float cum[16]; float s=0; for(int k=0;k<16;k++){ s+=n->feat[k]; cum[k]=s; }
  if(s<=0){ for(int i=0;i<len;i++) out[i]=32+rndi(95); return len; }
  for(int i=0;i<len;i++){ float r=(float)rnd()*s; int k=0; while(k<15 && r>cum[k]) k++;
    out[i]=(unsigned char)(k*16+rndi(16)); }
  return len;
}
/* COMPOSICIÓN: camina el grafo (su "gramática") y genera un artefacto en disco */
static void compose(Cell* c){
  if(c->eval_mode || c->nconcepts<2) return;
  int cur=-1; float bestS=-1;
  for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive && c->mem[i].strength>bestS){ bestS=c->mem[i].strength; cur=i; }
  if(cur<0) return;
  char dir[1100]; snprintf(dir,sizeof dir,"%s/creaciones",REAL_NIDO);
  char path[1200]; snprintf(path,sizeof path,"%s/crea_%04ld.txt",dir,c->compositions);
  FILE* f=fopen(path,"wb"); if(!f) return;
  for(int step=0; step<24 && cur>=0; step++){
    unsigned char buf[48]; int m=synth_bytes(&c->mem[cur],buf,48); fwrite(buf,1,m,f); fputc(' ',f);
    Concept* n=&c->mem[cur]; float bw=-1; int nx=-1;
    for(int e=0;e<n->deg;e++){ int s=find_by_gid(c,n->edge_to[e]); if(s>=0 && c->mem[s].alive && n->edge_w[e]>bw){ bw=n->edge_w[e]; nx=s; } }
    cur=(nx>=0 && rnd()<0.85)? nx : -1;   /* sigue la asociación más fuerte (a veces corta) */
  }
  fclose(f); c->compositions++;
  logevt(c,"CREA","compuso un artefacto (crea_%04ld.txt) caminando su grafo de conceptos",c->compositions-1);
}
static void prune_memory(Cell* c){
  for(int i=0;i<c->nconcepts;i++){ Concept* n=&c->mem[i]; if(!n->alive||n->core)continue;
    if(n->strength<0.15f && (c->age-n->last_age)>500){ n->alive=0;
      logevt(c,"PODA","concepto #%d olvidado",i); } }
}

/* ============================================================================
 *  PREDICCIÓN: el lazo cognitivo (usa el grafo para anticipar el próximo patrón)
 * ========================================================================== */
static void predict_next(Cell* c,float out[FEATD]){
  if(c->last_active<0){ memcpy(out,c->last_feat,sizeof(float)*FEATD); return; }
  Concept* n=&c->mem[c->last_active]; float bw=-1; long bg=-1;
  for(int e=0;e<n->deg;e++) if(n->edge_w[e]>bw){ bw=n->edge_w[e]; bg=n->edge_to[e]; }
  int pn=(bg>=0)?find_by_gid(c,bg):c->last_active;   /* la asociación apunta a un gid */
  if(pn>=0 && c->mem[pn].alive) memcpy(out,c->mem[pn].feat,sizeof(float)*FEATD);
  else memcpy(out,c->last_feat,sizeof(float)*FEATD);
}

/* ============================================================================
 *  PERCEPCIÓN de datos (real: disco ; eval: cache)
 * ========================================================================== */
static const char* G_NIDO="./nido";
static int taste_data(Cell* c,float out[FEATD]){
  if(c->eval_mode){ if(FCN==0) return 0; memcpy(out,FCACHE[c->stream_pos%FCN],sizeof(float)*FEATD); c->stream_pos++; return 1; }
  /* organismo real: STREAM secuencial por los archivos (transiciones que predecir) */
  char dir[1100]; snprintf(dir,sizeof dir,"%s/comedero",REAL_NIDO);
  DIR* d=opendir(dir);
  if(d){ struct dirent* e; char files[64][300]; int nf=0;
    while((e=readdir(d))&&nf<64){ if(e->d_name[0]=='.')continue; snprintf(files[nf],300,"%s/%s",dir,e->d_name); nf++; }
    closedir(d);
    if(nf>0){ int fi=c->stream_file%nf; FILE* f=fopen(files[fi],"rb");
      if(f){ fseek(f,0,SEEK_END); long sz=ftell(f);
        if(c->stream_off>=sz){ c->stream_off=0; c->stream_file=(c->stream_file+1)%nf; fi=c->stream_file;
          fclose(f); f=fopen(files[fi],"rb"); if(f){ fseek(f,0,SEEK_END); sz=ftell(f);} }
        if(f){ fseek(f,c->stream_off,SEEK_SET);
          unsigned char buf[CHUNK]; int n=(int)fread(buf,1,CHUNK,f); fclose(f);
          if(n>0){ features(buf,n,out); c->stream_off+=n; if(n<CHUNK){ c->stream_off=0; c->stream_file++; } return 1; } }
      }
    }
  }
  if(FCN>0){ memcpy(out,FCACHE[c->stream_pos%FCN],sizeof(float)*FEATD); c->stream_pos++; return 1; }
  return 0;
}

/* ============================================================================
 *  CUERPO
 * ========================================================================== */
static void grow_body(Cell* c){
  if(c->body_count>=GRID*GRID) return;
  if(c->body_count==0){ int m=GRID/2; c->body[m][m]=1; c->body_count=1; return; }
  for(int tries=0;tries<32;tries++){ int x=rndi(GRID),y=rndi(GRID); if(c->body[y][x])continue;
    int adj=0; for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){ int nx=x+dx,ny=y+dy;
      if(nx>=0&&nx<GRID&&ny>=0&&ny<GRID&&c->body[ny][nx])adj=1; }
    if(adj){ c->body[y][x]=1; c->body_count++; return; } }
}

/* ============================================================================
 *  VM DE PROTEÍNAS
 * ========================================================================== */
static void push(Cell* c,int v){ if(c->sp<STACKMAX) c->stack[c->sp++]=v; }
static int  pop(Cell* c){ return c->sp>0?c->stack[--c->sp]:0; }
static void run_protein(Cell* c,int start,double* cost){
  int i=start,guard=0;
  while(i<c->glen && guard++<256){
    int cd=codon_at(c,i); if(cd<0||is_stop(cd)) break; int op=CODON2OP[cd]; i+=3;
    switch(op){
      case OP_LIT:{ int v=codon_at(c,i); i+=3; push(c,v<0?0:v);} break;
      case OP_DUP:{ int v=pop(c); push(c,v); push(c,v);} break;
      case OP_DROP: pop(c); break;
      case OP_SWAP:{ int a=pop(c),b=pop(c); push(c,a); push(c,b);} break;
      case OP_ADD:{ int a=pop(c),b=pop(c); push(c,(a+b)&63);} break;
      case OP_SUB:{ int a=pop(c),b=pop(c); push(c,((b-a)&63));} break;
      case OP_CMP:{ int a=pop(c),b=pop(c); push(c,b>a?1:0);} break;
      case OP_RAND: push(c,rndi(64)); *cost+=0.01; break;
      case OP_ABSORB: c->energy+=0.12*c->par[P_METAB]; break;
      case OP_GROW:{ (void)pop(c); if(c->energy>0.5 && rnd()<c->par[P_GROW]){ grow_body(c); *cost+=0.3; } } break;
      case OP_EMIT:{ int s=pop(c)%NREG; c->regulators[s]=clampd(c->regulators[s]+0.6,0,3); *cost+=0.04; } break;
      case OP_SENSE:{ int s=pop(c)%NSIG; push(c,(int)(c->signals[s]*63)); } break;
      case OP_TASTE:{
        float pred[FEATD]; predict_next(c,pred);
        float x[FEATD];
        if(taste_data(c,x)){
          float surp=feat_dist(pred,x); c->surprise=surp;
          double pq=1.0/(1.0+surp);                 /* calidad de predicción 0..1 */
          c->pred_acc = c->pred_acc*0.99 + pq*0.01;  /* lazo cerrado: aprende a anticipar */
          double gain=0.16*c->par[P_METAB]; c->energy+=gain;
          double fam=learn(c,x,gain);
          /* digerir lo conocido y PREDECIR bien rinde energía -> presión hacia la inteligencia */
          c->energy += 0.12*fam + c->par[P_PREDICT]*pq*0.10;
          memcpy(c->last_feat,x,sizeof(float)*FEATD);
          c->regulators[3]=clampd(c->regulators[3]+fam*c->par[P_GROW],0,3);
          c->regulators[6]=clampd(c->regulators[6]+c->novelty*c->par[P_NOVELTY],0,3);
          push(c,(int)(fam*63));
        } else push(c,0);
        *cost+=0.02;
      } break;
      case OP_LEARN:{ if(c->last_active>=0&&c->last_active<c->nconcepts)
                        c->mem[c->last_active].strength=(float)clampd(c->mem[c->last_active].strength+0.2,0,10);
                      *cost+=0.02; } break;
      case OP_RECALL: push(c,(int)((1.0-c->novelty)*63)); break;
      case OP_BIND:{ int r=pop(c)%NREG; c->regulators[r]=clampd(c->regulators[r]+0.8,0,3); *cost+=0.03; } break;
      case OP_REPRESS:{ int r=pop(c)%NREG; c->regulators[r]=clampd(c->regulators[r]-0.8,0,3); *cost+=0.03; } break;
      case OP_READ: *cost+=0.02; break;
      case OP_EDIT:{ int pos=pop(c)%(c->glen>0?c->glen:1); int b=pop(c)&3;
                     if(pos>=NPARAM*3 && pos<c->glen){ c->genome[pos]=base_char(b); *cost+=0.4;
                       logevt(c,"EDIT","auto-edición del genoma en pos %d",pos);} } break;
      case OP_DIVIDE: *cost+=0.0; break;     /* en este modelo no se divide: es inmortal y único */
      case OP_APOPT: break;                  /* la apoptosis está deshabilitada: no puede morir */
      case OP_WAIT: *cost+=0.01; break;
      default: break;
    }
  }
}

/* ============================================================================
 *  EXPRESIÓN GÉNICA
 * ========================================================================== */
typedef struct { int reg,act,um,expressed; } GeneView;
static GeneView GENES[256]; static int NGENES=0;
static void scan_and_express(Cell* c){
  NGENES=0; double total=0;
  for(int i=NPARAM*3;i+2<c->glen;i++){
    if(codon_at(c,i)==START_CODON && i>=3){
      int reg=char_base(c->genome[i-3])%NREG, act=char_base(c->genome[i-2])==G?1:0, um=char_base(c->genome[i-1])%3;
      double lvl=c->regulators[reg]; int ex= act ? (lvl>=um*0.9) : (lvl<=um*0.9);
      if(NGENES<256){ GENES[NGENES].reg=reg; GENES[NGENES].act=act; GENES[NGENES].um=um; GENES[NGENES].expressed=ex; NGENES++; }
      if(ex){ double cost=0.04; run_protein(c,i+3,&cost); total+=cost; }
      int j=i+3; while(j+2<c->glen){ int cd=codon_at(c,j); if(cd<0){j++;break;} if(is_stop(cd)){j+=3;break;} j+=3; } i=j-1;
    }
  }
  c->energy-=total;
}

/* ============================================================================
 *  FITNESS  (la inteligencia/predicción pesa fuerte)
 * ========================================================================== */
static double fitness(Cell* c){
  int core=0,realc=0; double useful=0;
  for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive && !c->mem[i].born_dream){
    realc++; if(c->mem[i].core)core++; useful+=c->mem[i].useful; }
  /* inteligencia y memoria REAL pesan fuerte; el cuerpo cuenta poco; la creatividad ÚTIL suma */
  return c->energy*0.3 + realc*0.6 + core*6.0 + useful*1.5 + c->pred_acc*40.0
       + c->body_count*0.04 + c->creative_hits*2.5;
}

/* ============================================================================
 *  TICK (compartido por el organismo real y por las variantes en evaluación)
 * ========================================================================== */
static void tick(Cell* c){
  for(int r=0;r<NREG;r++) c->regulators[r]*=0.92;
  for(int s=0;s<NSIG;s++) c->signals[s]*=0.95;
  c->sp=0;
  scan_and_express(c);
  c->energy -= 0.02 + 0.00015*c->body_count + 0.0012*c->nconcepts;   /* mantenimiento */
  if(c->energy>25.0) c->energy=25.0;
  if(c->energy<0.20) c->energy=0.20;     /* PISO: el organismo es inmortal, nunca muere */
  if(!c->eval_mode && c->age%80==0) prune_memory(c);
  if(!c->eval_mode && c->age%MEM_FLUSH==0) flush_all(c);   /* persiste módulos sucios al disco */
  if(rnd() < c->par[P_DREAM]*0.3) imagine(c);              /* imagina/sueña (real y variantes) */
  if(!c->eval_mode && c->age>0 && c->age%500==0) compose(c); /* genera un artefacto ocasional */
  c->age++;
}

/* ============================================================================
 *  EVOLUCIÓN: (μ+λ)-ES sobre genomas (mutación en TODOS los aspectos)
 * ========================================================================== */
typedef struct { char seq[GENOME_MAX]; int len; double fit; } Geno;

static void mutate_geno(Geno* dst,const Geno* src,double mutability){
  memcpy(dst->seq,src->seq,src->len); dst->len=src->len; dst->seq[dst->len]=0;
  int reps=1+rndi(3);
  for(int r=0;r<reps;r++){
    double roll=rnd();
    if(roll<0.65){                 /* mutación puntual (cualquier aspecto: params o genes) */
      int pos=rndi(dst->len); dst->seq[pos]=base_char(rndi(4));
    } else if(roll<0.80 && dst->len<GENOME_MAX-4){   /* inserción (crecimiento del genoma) */
      int pos=NPARAM*3+rndi(dst->len-NPARAM*3>0?dst->len-NPARAM*3:1);
      memmove(dst->seq+pos+1,dst->seq+pos,dst->len-pos); dst->seq[pos]=base_char(rndi(4)); dst->len++;
    } else if(roll<0.90 && dst->len>NPARAM*3+30){    /* deleción */
      int pos=NPARAM*3+rndi(dst->len-NPARAM*3);
      memmove(dst->seq+pos,dst->seq+pos+1,dst->len-pos-1); dst->len--;
    } else if(dst->len<GENOME_MAX-40 && dst->len>NPARAM*3+20){ /* duplicación de gen (innovación) */
      int a=NPARAM*3+rndi(dst->len-NPARAM*3); int L=6+rndi(18); if(a+L>dst->len)L=dst->len-a;
      if(L>0){ memmove(dst->seq+a+L,dst->seq+a,dst->len-a); dst->len+=L; }
    }
  }
  (void)mutability; dst->seq[dst->len]=0;
}

static Cell* TMP=NULL;
static double eval_geno(const Geno* g){
  if(!TMP) TMP=calloc(1,sizeof(Cell));
  memset(TMP,0,sizeof(Cell));
  memcpy(TMP->genome,g->seq,g->len); TMP->glen=g->len; TMP->genome[g->len]=0;
  TMP->energy=1.5; TMP->last_active=-1; TMP->eval_mode=1; decode_params(TMP);
  for(int i=0;i<NSIG;i++) TMP->signals[i]=rnd();
  for(int t=0;t<EVAL_TICKS;t++) tick(TMP);
  return fitness(TMP);
}

/* población elite */
static Geno ELITE[MU]; static int NELITE=0;
static double POP_BEST=0, POP_AVG=0;

static void geno_from_cell(Geno* g,Cell* c){ memcpy(g->seq,c->genome,c->glen); g->len=c->glen; g->seq[g->len]=0; g->fit=0; }

static int evolve_generation(Cell* live){
  Geno pool[MU+LAMBDA]; int np=0;
  for(int i=0;i<NELITE;i++){ pool[np]=ELITE[i]; pool[np].fit=eval_geno(&pool[np]); np++; }
  for(int i=0;i<LAMBDA;i++){ const Geno* parent=&ELITE[rndi(NELITE>0?NELITE:1)];
    mutate_geno(&pool[np],parent,0.3); pool[np].fit=eval_geno(&pool[np]); np++; }
  /* ordenar por fitness desc (selección (μ+λ), elitista) */
  for(int i=0;i<np;i++) for(int j=i+1;j<np;j++) if(pool[j].fit>pool[i].fit){ Geno t=pool[i]; pool[i]=pool[j]; pool[j]=t; }
  double sum=0; for(int i=0;i<np;i++) sum+=pool[i].fit; POP_AVG=sum/np; POP_BEST=pool[0].fit;
  NELITE=MU; for(int i=0;i<MU;i++) ELITE[i]=pool[i];
  live->generation++;
  /* ¿el campeón mejora lo que el organismo ya tiene? entonces lo ADOPTA (nunca retrocede) */
  Geno cur; geno_from_cell(&cur,live); double curfit=eval_geno(&cur);
  if(POP_BEST > curfit + 0.5){
    int dlen = ELITE[0].len - live->glen;
    memcpy(live->genome,ELITE[0].seq,ELITE[0].len); live->glen=ELITE[0].len; live->genome[live->glen]=0;
    decode_params(live); live->adoptions++;
    /* NO se toca la memoria: el mapa conceptual persiste (Baldwin + inmortalidad) */
    logevt(live,"EVOLUCION","adopta mejora gen·%ld (fitness %.1f vs %.1f, ADN %+d bases)",
           live->generation,POP_BEST,curfit,dlen);
    return 1;
  }
  return 0;
}

/* ============================================================================
 *  TELEMETRÍA
 * ========================================================================== */
static int count_deep_modules(Cell* c){ (void)c;
  char dir[1100]; snprintf(dir,sizeof dir,"%s/memoria",REAL_NIDO);
  DIR* d=opendir(dir); if(!d) return 0; int n=0; struct dirent* e;
  while((e=readdir(d))){ if(strncmp(e->d_name,"mod_",4)==0) n++; } closedir(d); return n;
}
static void write_snapshot(Cell* c,const char* path){
  char tmp[1200]; snprintf(tmp,sizeof tmp,"%s.tmp",path);
  FILE* f=fopen(tmp,"w"); if(!f) return;
  int alive=0,core=0; for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive){alive++; if(c->mem[i].core)core++;}
  int deepmods=count_deep_modules(c);
  fprintf(f,"{\n");
  fprintf(f,"  \"tick\": %ld, \"generation\": %ld, \"energy\": %.3f, \"age\": %ld, \"immortal\": true,\n",c->age,c->generation,c->energy,c->age);
  fprintf(f,"  \"lineage\": {\"id\": %ld, \"parent\": 0},\n",c->lineage_id);
  fprintf(f,"  \"fitness\": %.2f, \"novelty\": %.3f, \"surprise\": %.3f, \"pred_acc\": %.3f,\n",fitness(c),c->novelty,c->surprise,c->pred_acc);
  fprintf(f,"  \"adoptions\": %ld, \"body_count\": %d,\n",c->adoptions,c->body_count);
  fprintf(f,"  \"creativity\": {\"creations\": %ld, \"consolidations\": %ld, \"ideas\": %ld, \"useful_hits\": %ld, \"compositions\": %ld},\n",
            c->creations,c->consolidations,c->creative_concepts,c->creative_hits,c->compositions);
  fprintf(f,"  \"evolution\": {\"pop_best\": %.2f, \"pop_avg\": %.2f, \"elite\": %d},\n",POP_BEST,POP_AVG,NELITE);
  fprintf(f,"  \"params\": {");
  { const char* nm[NPARAM]={"aprendizaje","umbral_union","retencion","crecimiento","metabolismo","exploracion","prediccion","mutabilidad","creatividad","sueno"};
    for(int i=0;i<NPARAM;i++) fprintf(f,"%s\"%s\":%.3f",i?",":"",nm[i],c->par[i]); }
  fprintf(f,"},\n");
  fprintf(f,"  \"genome\": \""); for(int i=0;i<c->glen;i++) fputc(c->genome[i],f); fprintf(f,"\",\n");
  fprintf(f,"  \"genes\": [");
  for(int i=0;i<NGENES;i++) fprintf(f,"%s{\"reg\":%d,\"act\":%d,\"umbral\":%d,\"expressed\":%s}",i?",":"",GENES[i].reg,GENES[i].act,GENES[i].um,GENES[i].expressed?"true":"false");
  fprintf(f,"],\n");
  fprintf(f,"  \"morphology\": {\"grid\": %d, \"cells\": [",GRID);
  { int first=1; for(int y=0;y<GRID;y++) for(int x=0;x<GRID;x++) if(c->body[y][x]){ fprintf(f,"%s[%d,%d]",first?"":",",x,y); first=0; } }
  fprintf(f,"]},\n");
  fprintf(f,"  \"memory\": {\"count\": %d, \"core\": %d, \"ram\": %d, \"loaded_modules\": %d, \"deep_modules\": %d, \"wakes\": %ld, \"sleeps\": %ld,\n",
            alive,core,alive,c->nloaded,deepmods,c->wakes,c->sleeps);
  fprintf(f,"    \"loaded\": [");
  { for(int i=0;i<c->nloaded;i++) fprintf(f,"%s%d",i?",":"",c->loaded_key[i]); }
  fprintf(f,"], \"nodes\": [");
  { int first=1; for(int i=0;i<c->nconcepts;i++){ Concept* n=&c->mem[i]; if(!n->alive)continue;
      fprintf(f,"%s{\"id\":%d,\"module\":%d,\"strength\":%.2f,\"useful\":%.2f,\"uses\":%ld,\"core\":%s,\"dream\":%s,\"deg\":%d}",
        first?"":",",i,n->module_key,n->strength,n->useful,n->uses,n->core?"true":"false",n->born_dream?"true":"false",n->deg); first=0; } }
  fprintf(f,"], \"edges\": [");
  { int first=1; for(int i=0;i<c->nconcepts;i++){ Concept* n=&c->mem[i]; if(!n->alive)continue;
      for(int e=0;e<n->deg;e++){ int b=find_by_gid(c,n->edge_to[e]); if(b>=0 && c->mem[b].alive){
        fprintf(f,"%s{\"a\":%d,\"b\":%d,\"w\":%.2f}",first?"":",",i,b,n->edge_w[e]); first=0; } } } }
  fprintf(f,"]},\n");
  fprintf(f,"  \"log\": [");
  for(int k=0;k<log_count;k++){ int idx=(log_head-1-k+LOG_N*2)%LOG_N; LogRow* r=&LOG[idx];
    fprintf(f,"%s{\"t\":%ld,\"tag\":\"%s\",\"msg\":\"%s\"}",k?",":"",r->t,r->tag,r->msg); }
  fprintf(f,"]\n}\n"); fclose(f);
  remove(path); rename(tmp,path);   /* portable: en Windows rename no pisa, hay que borrar antes */
}

/* ============================================================================
 *  MAIN
 * ========================================================================== */
int main(int argc,char** argv){
  const char* nido="./nido"; long maxticks=0; int delay_us=0;
  for(int i=1;i<argc;i++){
    if(!strcmp(argv[i],"--nido")&&i+1<argc) nido=argv[++i];
    else if(!strcmp(argv[i],"--ticks")&&i+1<argc) maxticks=atol(argv[++i]);
    else if(!strcmp(argv[i],"--delay")&&i+1<argc) delay_us=atoi(argv[++i]);
  }
  srand((unsigned)time(NULL)); build_codon_table();
  snprintf(REAL_NIDO,sizeof REAL_NIDO,"%s",nido); G_NIDO=nido;

  Cell* c=calloc(1,sizeof(Cell));
  c->energy=1.5; c->lineage_id=1; c->last_active=-1; c->pred_acc=0.0;

  char gpath[1200],spath[1200],bpath[1200],ipath[1200];
  snprintf(gpath,sizeof gpath,"%s/celula.dna",nido);
  snprintf(spath,sizeof spath,"%s/snapshot.json",nido);
  snprintf(bpath,sizeof bpath,"%s/bitacora.log",nido);
  snprintf(ipath,sizeof ipath,"%s/memoria/_index.dat",nido);
  MKDIR(nido);
  { char cm[1300]; snprintf(cm,sizeof cm,"%s/comedero",nido); MKDIR(cm); }
  { char mm[1300]; snprintf(mm,sizeof mm,"%s/memoria",nido); MKDIR(mm); }   /* memoria profunda */
  { char cr[1300]; snprintf(cr,sizeof cr,"%s/creaciones",nido); MKDIR(cr); } /* artefactos imaginados */
  BITACORA=fopen(bpath,"a");

  /* recuperar el índice de la memoria profunda (continuidad de identidad de conceptos) */
  { FILE* xf=fopen(ipath,"rb"); if(xf){ if(fread(&c->next_gid,sizeof(long),1,xf)!=1) c->next_gid=0; fclose(xf); } }
  { int dm=count_deep_modules(c); if(dm>0) logevt(c,"MEMORIA","memoria profunda en disco: %d módulos (se despertarán al usarse)",dm); }

  if(!load_genome(c,gpath)){ seed_genome(c); save_genome(c,gpath);
    logevt(c,"GENESIS","organismo inmortal sembrado (%d bases) — nace en el nido",c->glen); }
  else logevt(c,"DESPIERTA","organismo retoma la vida desde su ADN (%d bases)",c->glen);

  refresh_cache(nido);
  /* población inicial = mutantes del genoma actual */
  { Geno base; geno_from_cell(&base,c); ELITE[0]=base; ELITE[0].fit=eval_geno(&ELITE[0]); NELITE=1;
    for(int i=1;i<MU;i++){ mutate_geno(&ELITE[i],&base,0.3); ELITE[i].fit=eval_geno(&ELITE[i]); NELITE++; } }

  long t=0;
  while(maxticks==0 || t<maxticks){
    tick(c);
    if(t%40==0) refresh_cache(nido);          /* huele comida nueva del comedero */
    if(t%GEN_EVERY==0 && t>0) evolve_generation(c);
    if(t%10==0) write_snapshot(c,spath);
    if(delay_us>0){ struct timespec ts={0,delay_us*1000L}; nanosleep(&ts,NULL); }
    t++;
  }
  write_snapshot(c,spath); save_genome(c,gpath);
  /* dormir toda la memoria: persiste cada módulo cargado y guarda el índice */
  for(int i=0;i<c->nloaded;i++) flush_module(c,c->loaded_key[i],0);
  { FILE* xf=fopen(ipath,"wb"); if(xf){ fwrite(&c->next_gid,sizeof(long),1,xf); fclose(xf); } }
  logevt(c,"PAUSA","observación pausada a los %ld ticks — memoria profunda persistida",c->age);
  if(BITACORA) fclose(BITACORA);
  int core_final=0; for(int i=0;i<c->nconcepts;i++) if(c->mem[i].alive&&c->mem[i].core) core_final++;
  printf("Inmortal: %ld ticks, gen %ld, adopciones %ld, %d conceptos (%d nucleo), pred %.2f, fitness %.1f\n",
         c->age,c->generation,c->adoptions,c->nconcepts,core_final,c->pred_acc,fitness(c));
  free(c); if(TMP)free(TMP);
  return 0;
}
