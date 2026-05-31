# WilDeep - Protocolo de Segurança Avançado

## Visão Geral
O WilDeep é um protocolo de comunicação seguro que fragmenta dados em **500 partes**, utilizando **Shamir's Secret Sharing** (necessário apenas 200 partes para reconstruir), criptografia visual baseada em **cores RGB** e envio paralelo por múltiplas rotas.

## Características
- 🔒 **Fragmentação extrema**: 500 fragmentos por arquivo
- 🧩 **Tolerância a falhas**: Recupera dados mesmo perdendo 300 fragmentos (K=200)
- 🎨 **Criptografia visual**: Mapeamento byte → cor RGB com chave aleatória
- 🔄 **Inversão de bits**: Camada extra de ofuscação
- ⚡ **Envio paralelo**: Threads simultâneas para cada fragmento
- 🗺️ **Roteamento inteligente**: Seleciona rotas mais rápidas por latência

## Tecnologias
- **Linguagem**: C (GCC 15.2.0)
- **Concorrência**: pthreads
- **Build**: Makefile profissional
- **Controle de versão**: Git

## Compilação
```bash
make          # Compila o projeto
make clean    # Remove arquivos objeto
make test     # Executa com arquivo de teste
