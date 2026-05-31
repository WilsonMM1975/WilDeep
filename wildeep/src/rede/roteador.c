/**
 * roteador.c - Descoberta de rotas e envio paralelo
 */

#include "../../include/wildeep.h"

// Estrutura auxiliar para envio paralelo
typedef struct {
    Fragmento *fragmento;
    RotaInfo *rota;
    int done;
} EnvioTask;

static float _medir_latencia(const char *destino) {
    (void)destino;
    return (rand() % 190) + 10;
}

static float _medir_jitter(const char *destino) {
    (void)destino;
    return (rand() % 50) / 10.0;
}

static float _medir_perda(const char *destino) {
    (void)destino;
    return (rand() % 10) / 100.0;
}

RotaInfo* descobrir_rotas(const char *destino, int *num_rotas) {
    printf("[ROTEADOR] Descobrindo rotas para %s...\n", destino);
    
    RotaInfo *rotas = malloc(MAX_ROTAS * sizeof(RotaInfo));
    *num_rotas = 0;
    
    for (int i = 0; i < MAX_ROTAS; i++) {
        RotaInfo rota;
        strcpy(rota.destino, destino);
        
        rota.latencia_ms = _medir_latencia(destino);
        rota.jitter_ms = _medir_jitter(destino);
        rota.perda_pct = _medir_perda(destino);
        rota.num_saltos = (rand() % 15) + 5;
        
        snprintf(rota.rota_completa, sizeof(rota.rota_completa),
                 "192.168.1.1 -> 10.0.0.1 -> %s", destino);
        
        rota.pontuacao = calcular_pontuacao(&rota);
        
        rotas[*num_rotas] = rota;
        (*num_rotas)++;
    }
    
    // Ordena por pontuação (melhor primeiro)
    for (int i = 0; i < *num_rotas - 1; i++) {
        for (int j = i + 1; j < *num_rotas; j++) {
            if (rotas[i].pontuacao < rotas[j].pontuacao) {
                RotaInfo temp = rotas[i];
                rotas[i] = rotas[j];
                rotas[j] = temp;
            }
        }
    }
    
    printf("[ROTEADOR] Encontradas %d rotas. Melhor: %.1f ms (nota %.1f)\n", 
           *num_rotas, rotas[0].latencia_ms, rotas[0].pontuacao);
    
    return rotas;
}

float calcular_pontuacao(RotaInfo *rota) {
    float nota_latencia = 1.0f - (rota->latencia_ms / 300.0f);
    float nota_jitter = 1.0f - (rota->jitter_ms / 100.0f);
    float nota_perda = 1.0f - (rota->perda_pct / 10.0f);
    
    if (nota_latencia < 0) nota_latencia = 0;
    if (nota_jitter < 0) nota_jitter = 0;
    if (nota_perda < 0) nota_perda = 0;
    
    return (nota_latencia * 0.4f + nota_jitter * 0.2f + nota_perda * 0.4f) * 100;
}

static void* _envio_worker(void *arg) {
    EnvioTask *task = (EnvioTask*)arg;
    
    usleep((useconds_t)(task->rota->latencia_ms * 1000));
    
    printf("[ENVIO] Fragmento %d enviado via %s (%.1f ms)\n", 
           task->fragmento->id, task->rota->destino, task->rota->latencia_ms);
    
    task->done = 1;
    return NULL;
}

void enviar_fragmento_paralelo(Fragmento *frag, RotaInfo *rota) {
    Fragmento *frag_copy = malloc(sizeof(Fragmento));
    memcpy(frag_copy, frag, sizeof(Fragmento));
    
    RotaInfo *rota_copy = malloc(sizeof(RotaInfo));
    memcpy(rota_copy, rota, sizeof(RotaInfo));
    
    EnvioTask *task = malloc(sizeof(EnvioTask));
    task->fragmento = frag_copy;
    task->rota = rota_copy;
    task->done = 0;
    
    pthread_t thread;
    pthread_create(&thread, NULL, _envio_worker, task);
    pthread_detach(thread);
}
