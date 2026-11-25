# Ghost (in the shell) — Arquitetura e Decisões de Implementação

**Disciplina:** Compiladores (UFRPE) — Etapa 3 (Entrega Final)  
**Equipe:** Deborah Camilly Barbosa Leal Silva · Emanoel Asaph Leite de Oliveira · José Guilherme Torres Lima

Este documento descreve a arquitetura interna do interpretador da linguagem **Ghost (in the shell)**, a responsabilidade de cada módulo em `src/`, as principais decisões de implementação e as limitações conhecidas. A especificação formal da sintaxe e da semântica encontra-se em [gramatica.md](gramatica.md).

---

## 1. Visão Geral do Pipeline

O executável `./ghost` é um **interpretador puro baseado em Árvore Sintática Abstrata (AST)**, escrito em C11 com **Flex** (análise léxica) e **Bison** (análise sintática). Todo programa passa por quatro fases sequenciais coordenadas por `src/main.c`:

```text
arquivo.ghost (ou stdin)
   │
   ▼
[1. Análise Léxica]      src/lexer.l (Flex)
   │  Tokens + (linha, coluna) + detecção de erros léxicos
   ▼
[2. Análise Sintática]   src/parser.y (Bison LALR(1), 0 conflitos)
   │  Construção da AST explícita (src/ast.c/.h)
   ▼
[3. Análise Semântica]   src/semantic.c + src/symtab.c
   │  Tabela de símbolos com escopos + checagem de tipos e contexto
   ▼
[4. Interpretação AST]   src/interpreter.c
   │  Execução tree-walk + integração com o SO (system / popen)
   ▼
Saída padrão + código de saída (0, eject ou 1 em erro ICE)
```

**Propriedade fail-fast:** se qualquer erro for detectado nas fases 1, 2 ou 3, o pipeline é interrompido antes da fase 4. Nenhum comando `shell` chega a ser disparado para um script inválido, prevenindo execuções parciais destrutivas.

---

## 2. Organização dos Módulos (`src/`)

| Arquivo | Responsabilidade |
|---|---|
| `src/types.h` | Enumeração `DataType` (`TYPE_CORE`, `TYPE_FLUX`, `TYPE_STATE`, `TYPE_STREAM`, `TYPE_NONE`, `TYPE_UNKNOWN`) e conversão para texto (`type_to_string`). |
| `src/error.h` / `src/error.c` | Sistema de diagnósticos **Barreira ICE** (`ice_error`, `ice_get_error_count`). Formata todas as falhas como `[GHOST::ICE_FAULT] erro <fase> na linha L, coluna C: <descrição>`. |
| `src/lexer.l` | Especificação Flex. Reconhece as 31 palavras reservadas, operadores compostos (`<~`, `=/=`, `<=`, `>=`), literais (`core`, `flux`, `state`, `stream`), comentários `//` e `/* ... */` (via estado exclusivo `%x COMMENT_BLOCK`) e shebang na linha 1. Mantém `token_line` e `token_col` via `YY_USER_ACTION`. |
| `src/parser.y` | Especificação Bison com `%define parse.error verbose`. Define precedência/associatividade dos operadores e constrói os nós da AST nas ações semânticas. Gera parser LALR(1) com **0 conflitos shift/reduce e 0 reduce/reduce** (verificável em `src/parser.output`). |
| `src/ast.h` / `src/ast.c` | Define os 27 tipos de nós da AST (`ASTNodeType`), listas encadeadas de comandos/expressões (`ASTNodeList`) e parâmetros (`ParamNode`), construtores, impressão formatada (`ast_print` para `--ast`) e liberação recursiva de memória (`ast_free`). |
| `src/symtab.h` / `src/symtab.c` | Tabela de símbolos para a fase semântica, estruturada como pilha encadeada de escopos (`Scope -> parent`). Já inicializa o escopo global com o registrador imutável `signal: core`. |
| `src/semantic.h` / `src/semantic.c` | Percorre a AST em duas passagens no nível raiz (primeiro registra todos os `protocol`, depois valida os comandos e corpos), verifica tipagem forte, promoção `core -> flux`, escopos de bloco, aridade/tipos de chamadas e contexto de `cloak`, `bypass` e `yield`. |
| `src/interpreter.h` / `src/interpreter.c` | Interpretador tree-walk sobre a AST. Gerencia memória em tempo de execução (`RuntimeScope` e `Value`), controle de fluxo (`FlowSignal`), pilha de chamadas de protocolos (`MAX_CALL_DEPTH = 500`), argumentos CLI (`arg(n)`) e chamadas ao sistema operacional (`system`, `popen`, `access`, `getenv`). |
| `src/main.c` | Interface de linha de comando: flags `--tokens`, `--ast`, `--check`, `--version`, `--help`, leitura de arquivo ou `stdin` (`-`) e repasse dos argumentos do script. |

---

## 3. Principais Decisões de Implementação

### 3.1 Modalidade: Interpretador AST (Tree-Walk)
Como Ghost é uma linguagem de script voltada à automação de terminal, o tempo real de execução é dominado por I/O nos utilitários externos invocados (`cp`, `rm`, `make`, `df`, `sleep`). A interpretação direta sobre a AST elimina a etapa de geração de código/compilação para o usuário final, preservando o ciclo imediato *editar → executar* típico de scripts, mas mantendo a segurança de uma linguagem estaticamente tipada.

### 3.2 Gramática Livre de Conflitos (LALR(1))
Três escolhas sintáticas garantem um parser Bison sem ambiguidades:
1. **Blocos explícitos `dive ... surface`:** eliminam o problema clássico do *dangling else* (`otherwise` sempre se associa ao `when ... surface` imediatamente anterior).
2. **Separação entre `<~` e `=`:** atribuição usa exclusivamente `<~` e comparação usa `=`, impedindo a ambiguidade entre atribuição e igualdade.
3. **Palavra-chave `exec` para chamadas como comando:** toda instrução que começa com `ID` é inequivocamente uma atribuição (`ID <~ expr!`); chamadas de protocolo pelo efeito colateral usam `exec proto(...)!`.

### 3.3 Representação da AST
A AST usa uma estrutura única (`struct ASTNode`) com etiqueta `ASTNodeType`, linha/coluna de origem, `expr_type` anotado pelo analisador semântico e uma `union` discriminada para os campos específicos de cada comando ou expressão. Essa abordagem mantém a alocação simples em C padrão e facilita a travessia tanto no semântico quanto no interpretador.

### 3.4 Duas Passagens para Protocolos
Tanto `semantic.c` quanto `interpreter.c` fazem uma varredura inicial sobre `AST_PROGRAM` para registrar todos os nós `AST_PROTOCOL` antes de analisar ou executar os demais comandos. Isso permite:
- recursão direta e mútua entre protocolos (ex.: `fatorial(n - 1)` em `exemplos/05_protocolos_e_lacos.ghost`);
- invocar um protocolo antes do ponto do arquivo em que ele foi declarado.

### 3.5 Controle de Fluxo no Interpretador (`FlowSignal`)
Em vez de usar `setjmp`/`longjmp`, desvios de fluxo (`cloak!`, `bypass!`, `yield`, `eject`) são propagados de forma estruturada pelo campo `g_ctx.flow` (`FLOW_NORMAL`, `FLOW_CLOAK`, `FLOW_BYPASS`, `FLOW_YIELD`, `FLOW_EJECT`). Cada bloco e laço interrompe imediatamente sua iteração ao detectar `flow != FLOW_NORMAL`:
- `cycle`, `sweep` e `route` consomem `FLOW_CLOAK` (interrompe o laço) e `FLOW_BYPASS` (avança para a próxima iteração), mas propagam `FLOW_YIELD` e `FLOW_EJECT`.
- `call_protocol` consome `FLOW_YIELD` (recuperando `g_ctx.ret_value`) e propaga `FLOW_EJECT` até o encerramento do programa.

### 3.6 Curto-Circuito, Efeitos e Integração com o Sistema Operacional
- **Curto-circuito lógico:** `with` e `either` avaliam o operando direito apenas quando necessário (`src/interpreter.c`), evitando efeitos colaterais ou divisões por zero desnecessárias.
- **`whisper` atômico:** avalia todas as expressões da lista antes de imprimir qualquer caractere, evitando saída parcial caso alguma expressão gere erro em tempo de execução (ex.: divisão por zero).
- **`shell` em duas formas:** como comando (`shell "cmd"!`) usa `system()` para enviar a saída direto ao terminal; como expressão (`shell("cmd")`) usa `popen(..., "r")` com buffer dinâmico, remove a quebra de linha final e devolve um `stream`. Em ambos os casos, o código de retorno do processo é gravado no registrador imutável `signal`.
- **Proteção de pilha:** chamadas aninhadas de protocolos são limitadas por `MAX_CALL_DEPTH` (500) para transformar recursão infinita em erro ICE controlado em vez de *stack overflow*.

---

## 4. Testes e Reprodutibilidade

O projeto é compilado e validado via `Makefile` (requer apenas `gcc`, `make`, `flex` e `bison`):

```bash
make clean && make   # gera parser (Bison), lexer (Flex) e binário ./ghost
make test            # executa os 44 testes da suíte automatizada
make exemplos        # executa os 5 exemplos em pasta temporária isolada
make demo            # demonstração rápida (válido + erro de execução)
```

A suíte em `tests/suite/` (orquestrada por `rodar_testes.sh`) cobre **44 casos** divididos em:
- **7 programas válidos** (`validos/`): cenários reais, operadores, precedência e estruturas completas;
- **9 erros léxicos** (`erros_lexicos/`): acento em identificador, string/comentário não fechado, caracteres inválidos, identificador iniciado por dígito, shebang fora da linha 1, ponto flutuante incompleto;
- **16 erros sintáticos** (`erros_sintaticos/`): operando faltando, parêntese não fechado, falta de `!`, bloco sem `surface`, `=` em atribuição, relacionais encadeados, atribuição a `signal`, protocolo aninhado, chamada sem `exec`, entre outros;
- **12 erros semânticos e de execução** (`erros_semanticos/`): variável não declarada, `shell` com `core`, incompatibilidade de tipos, condição não-`state`, concatenação `&` com `core`, `cloak` fora de laço, `yield` fora de protocolo, redeclaração, tipo de retorno errado, aridade errada, protocolo sem retorno em expressão e divisão por zero em tempo de execução.

Cada teste inválido verifica automaticamente a **fase do erro** e a **linha exata** reportada pela Barreira ICE.

---

## 5. Limitações Conhecidas e Trabalhos Futuros

Como transparência técnica da entrega, mapeamos as seguintes limitações conhecidas da versão `1.0.0`:

1. **Isolamento de escopo em protocolos:** tanto `semantic.c` quanto `interpreter.c` encadeiam o escopo de `protocol` abaixo do escopo global, permitindo leitura de implantes globais declarados antes do protocolo (divergência em relação ao isolamento estrito descrito na regra E4 de `docs/gramatica.md`).
2. **Análise de fluxo de retorno (`yield`):** o analisador semântico valida o tipo de cada `yield`, mas ainda não verifica se *todos* os caminhos de um protocolo com `returns T` executam `yield`.
3. **Variável de controle do `sweep`:** `semantic.c` ainda não marca a variável de controle como `is_readonly`, permitindo atribuição dentro do corpo; além disso, `sweep` itera apenas de forma crescente (`+1`).
4. **Validação estática das funções embutidas:** `exists` e `env` não validam aridade nem exigem `stream` no analisador semântico (tratando argumentos inválidos de forma defensiva em runtime).
5. **Strings e rastreamento de posição:** a regra léxica de string em `lexer.l` aceita quebra de linha dentro de `[^"\\]`; a contagem de coluna conta bytes (não codepoints UTF-8); e alguns nós construídos no `parser.y` registram linha/coluna do último token da regra em vez de usar `%locations` (`@1`).
6. **Segurança e ergonomia no `shell`:** `system()`/`popen()` delegam para `/bin/sh -c`, exigindo aspas ao manipular caminhos com espaço e cautela contra *shell injection* com entradas externas; também não há conversão nativa `core -> stream` (ex.: `str(n)`) para montar comandos com contadores numéricos.
7. **Limites fixos internos:** arrays estáticos limitam argumentos por chamada de protocolo (16), expressões por `whisper` (32), linha lida por `scan` (1023 bytes) e argumentos de CLI (64), além de processos encerrados por sinal (`!WIFEXITED`) retornarem `signal = 0`.

---

## 6. Declaração de Uso de Inteligência Artificial

Em conformidade com a Seção 8 das diretrizes da disciplina, ferramentas de inteligência artificial generativa foram utilizadas como apoio durante o desenvolvimento (estruturação de código em C, revisão de regras Flex/Bison, elaboração de casos de teste e revisão de documentação), sob fluxo controlado com leitura prévia da arquitetura, contratos explícitos entre módulos e validação contínua na suíte de 44 testes automatizados. Toda a arquitetura, decisões de linguagem e verificação final foram revisadas e validadas pela equipe.
