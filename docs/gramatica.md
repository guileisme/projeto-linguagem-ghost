# Ghost (in the shell) — Gramática Inicial e Decisões de Sintaxe e Semântica

Equipe: Deborah Camilly Barbosa Leal Silva · Emanoel Asaph Leite de Oliveira · José Guilherme Torres Lima

Este documento formaliza a gramática da linguagem Ghost e registra as decisões de projeto
tomadas para a sintaxe e para a semântica. A versão executável da gramática está em
`gramatica/gramatica.y` (Bison) e o vocabulário de tokens em `lexico/lexico.l` (Flex).

---

## 1. Visão geral do pipeline

```
arquivo.ghost ──► Léxico (Flex) ──► Sintático (Bison) ──► AST ──► Semântico ──► Interpretador
                  lexico.l          gramatica.y               (próxima etapa)
```

As três fases de validação (léxica, sintática e semântica) rodam **antes** de qualquer
comando ser executado. Um script com erro nunca chega a disparar um `shell`, o que evita
execuções parciais (por exemplo, apagar arquivos e só depois descobrir um erro de digitação).

---

## 2. Vocabulário léxico

| Classe | Padrão | Exemplos |
|---|---|---|
| Identificador (`ID`) | `[a-zA-Z_][a-zA-Z0-9_]*` (diferencia maiúsculas/minúsculas) | `origem`, `_tmp`, `alvo2` |
| Inteiro (`NUM_CORE`) | `[0-9]+` | `0`, `8080` |
| Real (`NUM_FLUX`) | `[0-9]+\.[0-9]+` | `3.14`, `0.05` |
| Texto (`STRING`) | entre aspas duplas, em uma única linha; escapes `\n \t \" \\` | `"df -h ."` |
| Comentário | `// ...` até o fim da linha, ou `/* ... */` em várias linhas | |
| Shebang | `#!...` somente na linha 1, coluna 1 | `#!/usr/bin/env ghost` |

**31 palavras reservadas:** `dive surface implant core flux state stream sync void when otherwise
cycle sweep from to route in cloak bypass whisper scan shell signal eject protocol returns yield
exec with either never`.

**Operadores e pontuação:** `<~ ! = =/= < > <= >= + - * / % & ( ) , :`

---

## 3. Gramática (EBNF)

Notação: `{ X }` = zero ou mais repetições, `[ X ]` = opcional, `|` = alternativa.
Terminais em MAIÚSCULAS são os tokens do léxico.

```ebnf
script        = { protocol_decl | comando } ;

protocol_decl = PROTOCOL ID "(" [ params ] ")" [ RETURNS tipo ] bloco ;
params        = ID ":" tipo { "," ID ":" tipo } ;

bloco         = DIVE { comando } SURFACE ;

comando       = declaracao
              | atribuicao
              | condicional
              | laco_cycle
              | laco_sweep
              | laco_route
              | saida
              | entrada
              | exec_shell
              | EXEC chamada "!"
              | CLOAK "!"
              | BYPASS "!"
              | YIELD [ expr ] "!"
              | EJECT [ expr ] "!" ;

declaracao    = IMPLANT ID ":" tipo [ "<~" expr ] "!" ;
tipo          = CORE | FLUX | STATE | STREAM ;
atribuicao    = ID "<~" expr "!" ;

condicional   = WHEN expr bloco { OTHERWISE WHEN expr bloco } [ OTHERWISE bloco ] ;
laco_cycle    = CYCLE expr bloco ;
laco_sweep    = SWEEP ID FROM expr TO expr bloco ;
laco_route    = ROUTE ID IN expr bloco ;

saida         = WHISPER expr { "," expr } "!" ;
entrada       = SCAN ID "!" ;
exec_shell    = SHELL expr "!" ;

chamada       = ID "(" [ expr { "," expr } ] ")" ;

expr          = expr EITHER expr
              | expr WITH expr
              | NEVER expr
              | expr ( "=" | "=/=" ) expr
              | expr ( "<" | ">" | "<=" | ">=" ) expr
              | expr ( "+" | "-" | "&" ) expr
              | expr ( "*" | "/" | "%" ) expr
              | "-" expr
              | "(" expr ")"
              | chamada
              | SHELL "(" expr ")"
              | ID | SIGNAL | NUM_CORE | NUM_FLUX | STRING | SYNC | VOID ;
```

A gramática de `expr` é ambígua de propósito (forma compacta); a ambiguidade é resolvida pela
tabela de precedência abaixo, aplicada no Bison com `%left`, `%nonassoc` e `%precedence`.
**O Bison gera o parser sem nenhum conflito shift/reduce ou reduce/reduce.**

### 3.1 Precedência e associatividade

| Nível | Operadores | Associatividade |
|---|---|---|
| 1 (mais alto) | `( )`, chamadas, `shell(...)` | — |
| 2 | `never`, `-` unário | à direita |
| 3 | `*` `/` `%` | à esquerda |
| 4 | `+` `-` `&` | à esquerda |
| 5 | `<` `>` `<=` `>=` | não associativo |
| 6 | `=` `=/=` | não associativo |
| 7 | `with` (E) | à esquerda |
| 8 (mais baixo) | `either` (OU) | à esquerda |

Exemplos de como o parser agrupa:

| Expressão | Agrupamento |
|---|---|
| `a + b * c` | `a + (b * c)` |
| `-a * b` | `(-a) * b` |
| `x < y with y < z` | `(x < y) with (y < z)` |
| `p either q with r` | `p either (q with r)` |
| `never exists(f)` | `never (exists(f))` |
| `never a = b` | `(never a) = b` — ver decisão S7 |

---

## 4. Decisões de sintaxe

**S1. Blocos com `dive ... surface`.** Todo bloco abre com `dive` e fecha com `surface`, sem
chaves nem indentação significativa. Isso elimina o problema do *else pendente*: um
`otherwise` sempre se liga ao `when ... surface` imediatamente anterior.

**S2. Instruções terminam com `!` (TRIGGER).** Quebras de linha não têm significado; uma
instrução pode ocupar várias linhas e várias instruções podem ficar na mesma linha.
Declarações de bloco (`when`, `cycle`, `sweep`, `route`, `protocol`) não levam `!`, pois
terminam em `surface`.

**S3. Atribuição com `<~`, comparação com `=`.** O símbolo `=` nunca atribui. Escrever
`x = 5!` é erro sintático, o que elimina a confusão clássica entre `=` e `==`.

**S4. Declaração obrigatória com tipo explícito.** Toda variável nasce em um `implant` com
tipo (`implant x: core!`). A inicialização é opcional; sem ela vale o valor padrão do tipo
(ver semântica).

**S5. Chamada de protocolo como instrução exige `exec`.** Uma linha que começa com
identificador é sempre uma atribuição (`ID <~ ...`). Para chamar um protocolo pelo efeito,
escreve-se `exec relatorio(x)!`. Isso torna a gramática LALR(1) sem ambiguidades e deixa
explícito, na leitura, que ali há uma chamada.

**S6. `shell` tem duas formas.** Como instrução (`shell "ls"!`) executa e imprime a saída;
como expressão (`shell("ls")`), com parênteses obrigatórios, captura a saída como `stream`.

**S7. `never` tem precedência alta (nível 2).** Assim `never exists(f)` funciona sem
parênteses. A consequência é que `never a = b` significa `(never a) = b`. Para negar uma
comparação, usa-se parênteses: `never (a = b)`. Essa escolha segue a Tabela 3 da proposta.

**S8. Relacionais não se encadeiam.** `1 < 2 < 3` é erro sintático (operadores não
associativos). A forma correta é `1 < 2 with 2 < 3`.

**S9. Protocolos só no nível principal.** Não é possível declarar um protocolo dentro de um
bloco. Eles podem aparecer antes ou depois do código que os chama.

**S10. `signal` é palavra reservada.** Por isso `signal <~ 0!` já é rejeitado na análise
sintática: a regra de atribuição exige um `ID` à esquerda.

**S11. Strings em uma única linha.** Uma string não pode atravessar uma quebra de linha.
Textos longos são montados com `&` ou com várias expressões no `whisper`.

---

## 5. Decisões de semântica

### 5.1 Tipos e valores padrão

| Tipo | Conteúdo | Valor padrão (sem `<~`) |
|---|---|---|
| `core` | inteiro | `0` |
| `flux` | real | `0.0` |
| `state` | lógico (`sync` / `void`) | `void` |
| `stream` | texto | `""` |

### 5.2 Regras de tipo dos operadores

| Operação | Operandos | Resultado |
|---|---|---|
| `+ - *` | `core` e `core` | `core` |
| `+ - * /` | `flux` com `flux` ou `core` (promoção) | `flux` |
| `/` | `core` e `core` | `core` (divisão inteira) |
| `%` | `core` e `core` | `core` |
| `-` unário | `core` ou `flux` | mesmo tipo |
| `&` | `stream` e `stream` | `stream` |
| `< > <= >=` | numéricos (`core`/`flux`) | `state` |
| `= =/=` | dois operandos do mesmo tipo (ou numéricos mistos) | `state` |
| `with either` | `state` e `state` | `state` |
| `never` | `state` | `state` |

**T1. Tipagem forte.** A única conversão implícita é `core` → `flux` em contas mistas.
Nunca há conversão automática entre texto e número: `"log_" & 3` é erro. Somar `stream` com
`+` também é erro; texto se junta com `&`.

**T2. Atribuição.** O tipo da expressão deve ser igual ao tipo do implante. A exceção é
atribuir `core` a um implante `flux` (promoção). Atribuir `flux` a `core` é erro.

### 5.3 Regras dos comandos

| Construção | Regra |
|---|---|
| `when`, `cycle` | a condição deve ser `state` |
| `sweep i from a to b` | `a` e `b` devem ser `core`; `i` é declarado automaticamente como `core`, vale só dentro do bloco e não pode ser atribuído; a faixa é inclusiva (`1 to 3` = 1, 2, 3) |
| `route item in expr` | `expr` deve ser `stream`; o laço percorre cada linha; `item` é declarado automaticamente como `stream`, vale só dentro do bloco |
| `cloak!` / `bypass!` | só dentro de `cycle`, `sweep` ou `route` |
| `yield` | só dentro de `protocol`; com `returns T`, exige `yield expr!` com `expr` do tipo `T`; sem `returns`, só `yield!` |
| `exec p(...)` | `p` deve existir; se tiver `returns`, o valor é descartado |
| `p(...)` em expressão | `p` deve ter `returns` |
| chamadas | número e tipos dos argumentos devem bater com os parâmetros |
| `shell expr` / `shell(expr)` | `expr` deve ser `stream` |
| `whisper` | aceita qualquer tipo; os valores são convertidos para texto na impressão |
| `scan x` | `x` deve estar declarado; a entrada é convertida para o tipo de `x` (erro de execução se não converter) |
| `eject [expr]` | `expr` deve ser `core`; sem valor, sai com código `0` |
| `signal` | `core`, somente leitura; vale `0` antes do primeiro `shell` |

### 5.4 Escopo

**E1. Escopo de bloco.** Cada `dive ... surface` abre um novo escopo. Um implante só é
visível a partir da sua declaração, dentro do bloco onde foi declarado e nos blocos internos.

**E2. Sem redeclaração no mesmo escopo.** Declarar `x` duas vezes no mesmo bloco é erro.
Redeclarar em um bloco interno (sombreamento) é permitido.

**E3. Protocolos são globais.** Todos os protocolos são registrados antes da análise do
restante do script, então podem ser chamados antes da sua declaração. Nomes de protocolos
não podem repetir nem coincidir com as funções embutidas.

**E4. Corpo de protocolo isolado.** Dentro de um protocolo só são visíveis os parâmetros,
os implantes locais e os outros protocolos. Os implantes do script principal não são
acessíveis; os dados entram por parâmetro. Isso torna os protocolos reutilizáveis e
previsíveis.

### 5.5 Funções embutidas

| Função | Parâmetro | Retorno | Significado |
|---|---|---|---|
| `arg(n)` | `core` | `stream` | n-ésimo argumento da linha de comando (`""` se não existir) |
| `env(nome)` | `stream` | `stream` | variável de ambiente (`""` se não existir) |
| `exists(caminho)` | `stream` | `state` | `sync` se o arquivo ou diretório existir |

Elas são identificadores comuns no léxico; o analisador semântico as conhece de antemão,
como protocolos pré-declarados.

### 5.6 Erros de execução

Erros que só aparecem com valores reais, durante a interpretação: divisão ou resto por zero,
falha de conversão no `scan`. Interrompem o script com o diagnóstico ICE e código de saída
diferente de zero. Um `shell` que falha **não** interrompe o script: o código fica em
`signal` para o programa decidir o que fazer.

---

## 6. Formato dos diagnósticos (Barreira ICE)

```
[GHOST::ICE_FAULT] erro <fase> na linha L, coluna C: <descrição>
```

A coluna conta caracteres, não bytes (acentos em strings não deslocam a posição).
O léxico reporta todos os erros do arquivo de uma vez; o sintático para no primeiro erro.

Exemplos reais produzidos pela implementação atual:

```
[GHOST::ICE_FAULT] erro léxico na linha 1, coluna 9: caractere 'ç' é inválido na formação de identificadores
[GHOST::ICE_FAULT] erro sintático na linha 1, coluna 25: esperava um valor operando após '+', mas encontrou '!'
[GHOST::ICE_FAULT] erro sintático na linha 2, coluna 14: parêntese '(' aberto e não fechado antes de 'dive'
[GHOST::ICE_FAULT] erro sintático na linha 2, coluna 3: esperava '<~', mas encontrou '='
```
