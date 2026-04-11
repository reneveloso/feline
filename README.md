# FELINE - Fast rEfined onLINE search

Implementacao em C++ do algoritmo FELINE para consultas de alcancabilidade em grafos dirigidos, conforme descrito no artigo:

> **Reachability Queries in Very Large Graphs: A Fast Refined Online Search Approach**
> Veloso, R. R.; Cerf, L.; Meira Jr, W.; Zaki, M. J.
> EDBT 2014. DOI: 10.5441/002/edbt.2014.46

Dado um grafo dirigido G e dois vertices u e v, FELINE responde eficientemente se existe um caminho de u a v.

## Como funciona

FELINE constroi um indice que associa cada vertice a coordenadas (X, Y) em um plano 2D (Dominance Drawing). A propriedade fundamental e:

- Se u alcanca v, entao `X[u] <= X[v]` **e** `Y[u] <= Y[v]`
- Se essa relacao **nao** vale, u **nao** alcanca v (decidido em tempo constante)

Quando a relacao vale mas nao e conclusiva, uma busca DFS com poda e realizada em um subgrafo reduzido.

### Filtros aplicados nas consultas

1. **Positive-cut**: intervalos min-post de uma spanning tree decidem alcancabilidade positiva em O(1)
2. **Negative-cut**: relacao de dominancia (X, Y) descarta nao-alcancabilidade em O(1)
3. **Level filter**: vertices no mesmo nivel ou nivel inferior nao podem ser alcancados

## Arquitetura interna

### Estruturas de dados

**Grafo CSR (Compressed Sparse Row)** -- representacao compacta e cache-friendly:

```cpp
using vertex_t = uint32_t;
using edge_t   = uint64_t;

struct CSRGraph {
    uint32_t n;                      // vertices
    uint64_t m;                      // arestas
    std::vector<edge_t> out_begin;   // tamanho n+1, inicio dos sucessores
    std::vector<vertex_t> out_adj;   // tamanho m, sucessores contiguos
};
```

**Indice FELINE** -- 5 arrays de tamanho |V| (espaco linear):

```cpp
struct FELINEIndex {
    std::vector<uint32_t> x_rank;         // coordenada X (ordem topologica)
    std::vector<uint32_t> y_rank;         // coordenada Y (heuristica Kornaropoulos)
    std::vector<uint32_t> interval_start; // s_u (positive-cut, min-post)
    std::vector<uint32_t> interval_end;   // e_u (positive-cut, min-post)
    std::vector<uint32_t> level;          // filtro de nivel
};
```

### Pipeline de construcao do indice

1. **Condensacao SCC** (`graph.cpp`): Tarjan **iterativo** com stack explicita. Colapsa componentes fortemente conexos em vertices unicos, produzindo um DAG. Complexidade: O(|V| + |E|).

2. **Coordenada X** (`index.cpp`): Kahn's algorithm (ordenacao topologica por BFS). `x_rank[u]` = posicao de u na ordenacao. Complexidade: O(|V| + |E|).

3. **Coordenada Y** (`index.cpp`): Heuristica de Kornaropoulos -- segunda ordenacao topologica onde, a cada passo, seleciona-se a raiz com maior x_rank (max-heap). Minimiza localmente o numero de falsos-positivos. Complexidade: O(|V| log |V| + |E|).

4. **Intervalos min-post** (`index.cpp`): DFS **iterativa** extrai spanning tree/forest e calcula post-order. `interval_end[u] = post_order[u]`. `interval_start[u] = min(interval_start[filhos])` ou `interval_end[u]` se folha. Checagem: `I_v ⊆ I_u` <=> `start[u] <= start[v] && end[v] <= end[u]`. Complexidade: O(|V| + |E|).

5. **Niveis** (`index.cpp`): processa vertices em ordem topologica. `level[v] = 0` se raiz, senao `level[v] = max(level[u] + 1)` para predecessores u. Complexidade: O(|V| + |E|).

### Algoritmo de consulta (Algorithm 3 do artigo)

Para `reachable(u, v)`, DFS **iterativa** com tres filtros de poda:

1. **Positive-cut**: se `I_v ⊆ I_u` -> retorna true (O(1))
2. **Reflexividade**: se `u == v` -> retorna true
3. **Negative-cut + level filter**: se `x[u] <= x[v] AND y[u] <= y[v] AND level[u] < level[v]`, explora sucessores; senao, poda o ramo

Todos os algoritmos usam stacks explicitas (nunca recursao) para suportar grafos com milhoes de vertices sem stack overflow.

## Requisitos

- C++17 (g++ >= 7 ou clang++ >= 5)
- CMake >= 3.14
- Python 3 com `matplotlib` e `numpy` (apenas para visualizacao)

## Compilacao

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Para compilacao com sanitizers de debug:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## Testes

Executar da raiz do projeto (os testes referenciam caminhos relativos):

```bash
cd /caminho/para/feline
./build/feline_tests
```

Os 16 testes unitarios cobrem:

| Grupo | Testes | O que verifica |
|-------|--------|----------------|
| Carga do grafo | 2 | n, m corretos; adjacencia CSR |
| Condensacao SCC | 2 | DAG puro: identidade; ciclico: SCCs corretos, DAG condensado |
| Coordenadas X | 1 | Para toda aresta (u,v): x_rank[u] < x_rank[v]; ranks unicos em [0,n) |
| Coordenadas Y | 1 | Para toda aresta (u,v): y_rank[u] < y_rank[v]; ranks unicos |
| Intervalos min-post | 1 | Raiz contem todos intervalos; folhas: start == end |
| Niveis | 2 | Raizes com level 0; level cresce ao longo das arestas |
| Consultas | 4 | Casos especificos em cada grafo de teste |
| Exaustivo (BFS oracle) | 2 | Todos os pares verificados contra BFS naive |
| Exportacao CSV | 1 | CSV gerado tem todos os vertices com valores corretos |

Grafos de teste em `test/test_data/`: diamond (4v), chain (5v), tree (7v), cyclic (6v com 2 SCCs).

## Uso

### Processar consultas de alcancabilidade

```bash
./build/feline <grafo> <consultas> [saida]
```

Exemplo:

```bash
./build/feline test/test_data/diamond.txt queries.txt resultados.txt
```

Se o arquivo de saida for omitido, os resultados nao sao gravados (util para medir apenas o tempo).

A saida no stderr mostra tempos de cada fase:

```
Loading graph: test/test_data/diamond.txt
  Vertices: 4, Edges: 4 (0.27 ms)
SCC condensation...
  Components: 4, DAG edges: 4 (0.15 ms)
Building FELINE index...
  Index built (0.17 ms)
Processing queries: queries.txt
  Queries: 5, Total: 0.11 ms, Avg: 0.0224 ms/query
Total time: 0.70 ms
```

### Exportar indice para visualizacao

```bash
./build/feline --export-index <grafo> <indice.csv>
```

Exemplo:

```bash
./build/feline --export-index test/test_data/diamond.txt index.csv
```

### Gerar consultas com gabarito BFS

Gera pares aleatorios de vertices e executa BFS exaustiva para determinar a alcancabilidade real de cada par. O resultado e um arquivo com o gabarito (ground truth).

```bash
./build/feline --gen-queries <grafo> <saida.txt> <num_queries> [seed]
```

Exemplo:

```bash
./build/feline --gen-queries test/test_data/diamond.txt queries_bfs.txt 1000
```

O arquivo gerado tem o formato `origem destino resultado`:

```
1 3 1
3 0 0
2 3 1
0 0 1
```

O parametro `seed` (padrao: 42) controla a geracao aleatoria para reprodutibilidade.

### Verificar FELINE contra gabarito BFS

Dado um arquivo de consultas com gabarito (gerado pelo comando anterior), executa o FELINE sobre cada consulta e compara o resultado com o esperado.

```bash
./build/feline --verify <grafo> <consultas_com_gabarito.txt>
```

Exemplo:

```bash
./build/feline --verify test/test_data/diamond.txt queries_bfs.txt
```

Saida:

```
=== Verification Results ===
Total queries:      20
  Positive (BFS=1): 12
  Negative (BFS=0): 8

Correct:            20 (100.00%)
Wrong:              0 (0.00%)

Verification time:  0.09 ms (0.0046 ms/query)
```

Em caso de erros, o relatorio detalha falsos positivos (FELINE=1, BFS=0) e falsos negativos (FELINE=0, BFS=1). O comando retorna codigo de saida 1 se houver erros.

### Visualizar o indice 2D

```bash
python3 tools/plot_index.py index.csv -o grafico.png --edges grafo.txt
```

Opcoes:
- `-o arquivo.png` -- salvar em arquivo (PNG/SVG/PDF). Se omitido, abre janela interativa.
- `--edges grafo.txt` -- desenhar arestas do grafo como setas.
- `--no-labels` -- ocultar rotulos dos vertices (recomendado para grafos grandes).

## Formatos de arquivo

### Grafo de entrada

Arquivo texto com lista de arestas. Vertices sao inteiros 0-indexados.

```
<num_vertices> <num_arestas>
<u0> <v0>
<u1> <v1>
...
```

Exemplo (grafo diamante):

```
4 4
0 1
0 2
1 3
2 3
```

O grafo pode conter ciclos -- a condensacao SCC e aplicada automaticamente.

### Arquivo de consultas (modo padrao)

Um par de vertices por linha:

```
<origem> <destino>
<origem> <destino>
...
```

Exemplo:

```
0 3
3 0
1 2
```

### Arquivo de consultas com gabarito (modos --gen-queries / --verify)

Tres campos por linha: origem, destino e resultado da BFS (0 ou 1):

```
<origem> <destino> <alcancavel>
<origem> <destino> <alcancavel>
...
```

Exemplo:

```
1 3 1
3 0 0
2 3 1
0 0 1
```

### Arquivo de saida

Uma linha por consulta, com `1` (alcancavel) ou `0` (nao alcancavel):

```
1
0
0
```

### CSV do indice exportado

Cabecalho seguido de uma linha por vertice do DAG condensado:

```
vertex,x,y,level
0,0,0,0
1,1,2,1
2,2,1,1
3,3,3,2
```

Campos:
- `vertex` -- identificador do vertice no DAG condensado
- `x` -- coordenada X (posicao na primeira ordenacao topologica)
- `y` -- coordenada Y (posicao na ordenacao de Kornaropoulos)
- `level` -- nivel do vertice (maior distancia a partir de uma raiz)

## Estrutura do projeto

```
feline/
├── CMakeLists.txt          # Build system
├── README.md               # Este arquivo
├── include/feline/
│   ├── graph.hpp           # Grafo CSR, condensacao SCC
│   ├── index.hpp           # Indice FELINE
│   ├── query.hpp           # Motor de consultas, BFS oracle, verificacao
│   └── timer.hpp           # Medicao de tempo
├── src/
│   ├── graph.cpp           # Carga de grafos + Tarjan iterativo
│   ├── index.cpp           # X, Y, intervalos min-post, niveis
│   ├── query.cpp           # Algorithm 3 (DFS iterativa com poda), BFS oracle, geracao e verificacao
│   └── main.cpp            # CLI (query, export-index, gen-queries, verify)
├── tools/
│   └── plot_index.py       # Visualizacao 2D com matplotlib
└── test/
    ├── test_main.cpp       # 16 testes unitarios
    └── test_data/          # Grafos de teste
        ├── diamond.txt     # DAG diamante (4 vertices)
        ├── chain.txt       # Cadeia linear (5 vertices)
        ├── tree.txt        # Arvore binaria (7 vertices)
        └── cyclic.txt      # Grafo com ciclos (6 vertices, 2 SCCs)
```

## Complexidade

| Operacao | Tempo | Espaco |
|----------|-------|--------|
| Condensacao SCC | O(\|V\| + \|E\|) | O(\|V\| + \|E\|) |
| Construcao do indice | O(\|V\| log \|V\| + \|E\|) | O(\|V\|) |
| Consulta (negative-cut) | O(1) | - |
| Consulta (positive-cut) | O(1) | - |
| Consulta (pior caso) | O(\|V\| + \|E\|) | O(\|V\|) |

## Referencia

```bibtex
@inproceedings{veloso2014feline,
  title={Reachability Queries in Very Large Graphs: A Fast Refined Online Search Approach},
  author={Veloso, Ren{\^e} R. and Cerf, Lo{\"i}c and Meira Jr, Wagner and Zaki, Mohammed J.},
  booktitle={Proc. 17th International Conference on Extending Database Technology (EDBT)},
  pages={511--522},
  year={2014},
  doi={10.5441/002/edbt.2014.46}
}
```
