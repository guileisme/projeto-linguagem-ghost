# Ghost (in the shell)

Linguagem de script **cyberpunk** para automação de terminal, interpretada e estaticamente tipada.
Projeto da disciplina de Compiladores (UFRPE) — modalidade **interpretador**, implementado em C com Flex e Bison.

**Equipe:** Deborah Camilly Barbosa Leal Silva · Emanoel Asaph Leite de Oliveira · José Guilherme Torres Lima

## Um programa Ghost

```
implant total: core <~ 0!                 // variável (implant) com tipo
sweep i from 1 to 5 dive                  // laço de contagem; blocos: dive ... surface
  total <~ total + i!                     // atribuição (<~) e fim de instrução (!)
surface
when total > 10 dive                      // condicional
  whisper "total = ", total!              // saída
surface otherwise dive
  whisper "pouco"!
surface
shell "date"!                             // executa um comando do sistema
```

Tipos: `core` (inteiro), `flux` (real), `state` (`sync`/`void`), `stream` (texto).
Comandos: `implant`, `<~`, `when/otherwise`, `cycle`, `sweep`, `route`, `cloak`, `bypass`,
`whisper`, `scan`, `shell`, `eject`, `protocol/returns/yield/exec`. Funções embutidas: `env`, `arg`, `exists`.
A proposta completa da linguagem está em `docs/` e a gramática formal em `docs/gramatica.md`.

## Requisitos

`gcc`, `make`, `flex` e `bison`. No Ubuntu/Debian/WSL:

```bash
sudo apt install build-essential flex bison
```

## Compilar e usar

```bash
make                                  # compila e gera o executável ./ghost
./ghost programa.ghost                # executa um programa
./ghost programa.ghost arg1 arg2      # arg(1) e arg(2) devolvem "arg1" e "arg2"
./ghost --tokens programa.ghost       # só análise léxica: tabela de tokens
./ghost --ast programa.ghost          # mostra a árvore sintática (AST) e executa
./ghost --check programa.ghost        # léxica + sintática + semântica, sem executar
make clean                            # apaga os arquivos gerados
```

O programa passa por quatro fases, nesta ordem, e **nada é executado se uma fase anterior falhar**:
análise léxica (Flex) → análise sintática e construção da AST (Bison) → análise semântica
(tabela de símbolos e checagem de tipos) → interpretação da AST.
Os erros seguem o formato `[GHOST::ICE_FAULT] erro <fase> na linha L, coluna C: <descrição>`.

## Testes e exemplos

```bash
make test        # suíte com 44 testes (válidos e erros léxicos, sintáticos, semânticos e de execução)
make exemplos    # roda os programas de exemplo e mostra a saída de cada um
make demo        # demonstração rápida
```

- `tests/suite/` — suíte de testes. Cada arquivo inválido começa com `// ESPERADO: ...`; o teste passa
  se o erro for da fase certa e apontar a linha esperada. `./rodar_testes.sh` mostra PASSOU / FALHOU / PENDENTE.
- `exemplos/` — 5 programas comentados (`01` a `05`) e `rodar_exemplos.sh`.

## Estrutura

```
src/lexer.l        análise léxica (Flex)
src/parser.y       gramática e construção da AST (Bison)
src/ast.c/.h       nós da AST
src/symtab.c/.h    tabela de símbolos com escopos
src/semantic.c/.h  análise semântica (tipos, escopos, protocolos)
src/interpreter.c/.h  interpretador: executa a AST
src/error.c/.h     diagnósticos [GHOST::ICE_FAULT]
src/main.c         linha de comando
tests/suite/       suíte de testes      tests/valid, tests/invalid: testes iniciais
exemplos/          programas de exemplo e script que os executa
docs/              proposta da linguagem e gramática
```

## Estado atual (Etapa 2)

- [x] Gramática e decisões de sintaxe e semântica
- [x] Análise léxica
- [x] Análise sintática
- [x] Construção da AST
- [x] Tabela de símbolos e análise semântica
- [x] Interpretador (inclusive laços `sweep`/`route`, protocolos recursivos e `arg()`)
- [x] Exemplos de execução e testes iniciais (válidos e inválidos)

## Uso de inteligência artificial

Para o desenvolvimento, nós utilizamos I.A. generativa em fluxo agêntico, mas sob um harness de contexto e governança criado pelo Prof. Lucas Figueiredo. Esse harness estrutura o workspace com regras rigorosas (AGENTS.md e contextos locais):
  1. Leitura antes da escrita: A I.A. era forçada a entender a arquitetura do Flex e Bison antes de propor qualquer código.
  2. Contratos técnicos estritos: Garantia que as uniões da AST, as regras do Bison e a tabela de símbolos conversassem sem quebrar a tipagem.
  3. Validação contínua (TDD): Toda adição de código era imediatamente submetida à suíte de testes de regressão. Ou seja: a I.A. não gerou código solto; ela atuou como par de programação dentro de um ambiente controlado e auditável pelo harness.
