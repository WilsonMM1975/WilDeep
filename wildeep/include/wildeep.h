/**
 * wildeep.h - Cabeçalho principal do WilDeep
 * 
 * Este arquivo contém todas as estruturas de dados e configurações
 * globais usadas pelo sistema.
 */

#ifndef WILDEEP_H
#define WILDEEP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

// ============================================================
// CONFIGURAÇÕES GLOBAIS (Você pode ajustar)
// ============================================================

#define NUM_FRAGMENTOS 500      // Total de fragmentos (500 partes)
#define K_FRAGMENTOS 200        // Necessários para reconstruir (Shamir threshold)
#define TAMANHO_FRAGMENTO 1400  // Bytes por fragmento (menor que MTU 1500)
#define PRIME 2147483647        // Número primo grande para Shamir
#define MAX_ROTAS 100      // Máximo de rotas candidatas

// ============================================================
// ESTRUTURAS DE DADOS
// ============================================================

/**
 * CorRGB - Representa uma cor no espaço RGB (Red, Green, Blue)
 * Cada componente é um byte (0 a 255)
 */
typedef struct {
    unsigned char r, g, b;
} CorRGB;

/**
 * ChaveCores - Mapeia bytes (0-255) para cores RGB
 * É a chave da "criptografia de cores"
 */
typedef struct {
    CorRGB mapeamento_byte_para_cor[256];  // Byte -> Cor
    unsigned char mapeamento_cor_para_byte[256]; // Cor -> Byte (simplificado)
    unsigned char chave_mestra[32];        // Chave AES-256 (para derivar aleatoriedade)
} ChaveCores;

/**
 * ShareShamir - Representa uma parte (share) da chave mestra
 * Usando o algoritmo de Shamir's Secret Sharing (K de N)
 */
typedef struct {
    int x;                      // Coordenada X (geralmente 1..NUM_FRAGMENTOS)
    unsigned char y[256];       // Coordenada Y (o share em si)
    int tamanho;                // Tamanho útil do share
} ShareShamir;

/**
 * Fragmento - Cada uma das 500 partes da cápsula
 * Cada fragmento contém: dados visuais + share da chave + rota
 */
typedef struct {
    int id;                     // Identificador do fragmento (1 a 500)
    int total;                  // Total de fragmentos (sempre 500)
    
    // Parte 1: Dados visuais (pedaço da imagem RGB)
    unsigned char dados_visuais[TAMANHO_FRAGMENTO];
    int tamanho_visual;
    
    // Parte 2: Share do Shamir (parte da chave mestra)
    ShareShamir share;
    
    // Parte 3: Roteamento (source routing)
    char proximo_hop[16];       // Próximo destino (ex: "192.168.1.1")
    int checksum;               // Para verificar integridade
    
} Fragmento;

/**
 * RotaInfo - Métricas de uma rota candidata
 * Usado pelo módulo de radar para escolher o melhor caminho
 */
typedef struct {
    char destino[256];
    float latencia_ms;          // Tempo de ida e volta
    float jitter_ms;            // Variação da latência
    float perda_pct;            // Porcentagem de pacotes perdidos
    int num_saltos;             // Quantos roteadores no caminho
    char rota_completa[1024];   // Lista de IPs (ex: "A -> B -> C")
    float pontuacao;            // Nota final (0 a 100)
} RotaInfo;

// ============================================================
// FUNÇÕES PÚBLICAS (Declarações)
// ============================================================

// ---------- core/fragmentador.c ----------
int fragmentar_arquivo(const char *entrada, Fragmento *fragmentos);
int remontar_arquivo(Fragmento *fragmentos, int num_recebidos, const char *saida);

// ---------- core/shamir.c ----------
void shamir_split(const unsigned char *segredo, int tamanho, ShareShamir *shares, int num_shares, int k);
int shamir_recover(ShareShamir *shares, int num_shares, unsigned char *segredo, int *tamanho);

// ---------- seguranca/cores.c ----------
void gerar_chave_cores(ChaveCores *chave, const unsigned char *semente, int tamanho_semente);
unsigned char* dados_para_imagem(const unsigned char *dados, int tamanho, ChaveCores *chave, int *largura, int *altura);
unsigned char* imagem_para_dados(const unsigned char *imagem, int largura, int altura, ChaveCores *chave, int *tamanho);
void inverter_fragmento(unsigned char *dados, int tamanho);

// ---------- rede/roteador.c ----------
RotaInfo* descobrir_rotas(const char *destino, int *num_rotas);
float calcular_pontuacao(RotaInfo *rota);
void enviar_fragmento_paralelo(Fragmento *frag, RotaInfo *rota);

// ---------- main.c (funções auxiliares) ----------
void imprimir_ajuda(void);

void salvar_imagem_ppm(const unsigned char *imagem, int largura, int altura, const char *nome);

#endif // WILDEEP_H
