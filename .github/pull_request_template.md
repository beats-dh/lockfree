# Pull Request

## 📋 Descrição

<!-- Descreva brevemente as mudanças implementadas neste PR -->

### Tipo de Mudança

<!-- Marque o tipo de mudança que este PR representa -->

- [ ] 🐛 Bug fix (mudança que corrige um problema)
- [ ] ✨ Nova funcionalidade (mudança que adiciona funcionalidade)
- [ ] 💥 Breaking change (mudança que quebra compatibilidade)
- [ ] 📚 Documentação (mudanças apenas na documentação)
- [ ] 🎨 Refatoração (mudança que não corrige bug nem adiciona funcionalidade)
- [ ] ⚡ Performance (mudança que melhora performance)
- [ ] ✅ Testes (adição ou correção de testes)
- [ ] 🔧 Configuração (mudanças em arquivos de configuração)
- [ ] 🚀 CI/CD (mudanças no pipeline de CI/CD)

## 🔗 Issue Relacionada

<!-- Se este PR resolve uma issue, referencie aqui -->

Resolve #(número da issue)

## 🧪 Como Testar

<!-- Descreva como revisar e testar as mudanças -->

### Pré-requisitos

- [ ] vcpkg configurado
- [ ] CMake 3.20+
- [ ] Compilador C++20

### Passos para Testar

1. Clone o branch: `git checkout feature/branch-name`
2. Configure o projeto: `cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake`
3. Compile: `cmake --build build --config Release`
4. Execute os testes: `ctest --test-dir build`
5. Execute os benchmarks: `./build/bin/lockfree_benchmark`

### Casos de Teste Específicos

<!-- Liste casos de teste específicos para suas mudanças -->

- [ ] Teste A: Descrição do teste
- [ ] Teste B: Descrição do teste
- [ ] Teste C: Descrição do teste

## 📊 Impacto na Performance

<!-- Se aplicável, inclua resultados de benchmarks -->

### Antes

```
// Cole aqui os resultados dos benchmarks antes das mudanças
```

### Depois

```
// Cole aqui os resultados dos benchmarks depois das mudanças
```

### Análise

<!-- Analise os resultados e explique as mudanças de performance -->

## 🔄 Breaking Changes

<!-- Se houver breaking changes, liste-os aqui -->

- [ ] Não há breaking changes
- [ ] Mudanças na API pública:
  - Função/classe X foi removida/modificada
  - Parâmetro Y foi adicionado/removido
  - Comportamento Z foi alterado

### Guia de Migração

<!-- Se houver breaking changes, forneça um guia de migração -->

```cpp
// Código antigo
// ...

// Código novo
// ...
```

## 📝 Checklist

### Código

- [ ] Meu código segue as convenções de estilo do projeto
- [ ] Realizei uma auto-revisão do meu código
- [ ] Comentei meu código, especialmente em áreas difíceis de entender
- [ ] Minhas mudanças não geram novos warnings
- [ ] Adicionei testes que provam que minha correção é efetiva ou que minha funcionalidade funciona
- [ ] Testes novos e existentes passam localmente com minhas mudanças

### Documentação

- [ ] Fiz mudanças correspondentes na documentação
- [ ] Atualizei comentários no código quando necessário
- [ ] Atualizei o README.md se necessário
- [ ] Atualizei o CHANGELOG.md (se aplicável)

### Performance e Qualidade

- [ ] Verifiquei que não há vazamentos de memória
- [ ] Verifiquei que não há race conditions
- [ ] Executei benchmarks para verificar impacto na performance
- [ ] Testei em diferentes compiladores (se possível)
- [ ] Testei em diferentes sistemas operacionais (se possível)

### CI/CD

- [ ] Todos os checks do CI passaram
- [ ] Análise de código estático passou
- [ ] Testes de segurança passaram
- [ ] Verificação de formatação passou

## 🔍 Detalhes Técnicos

### Arquivos Modificados

<!-- Liste os principais arquivos modificados e o motivo -->

- `include/lockfree/file.hpp`: Descrição da mudança
- `examples/example.cpp`: Descrição da mudança
- `tests/test_file.cpp`: Descrição da mudança

### Dependências

- [ ] Não adicionei novas dependências
- [ ] Adicionei as seguintes dependências:
  - Dependência X: Motivo
  - Dependência Y: Motivo

### Compatibilidade

- [ ] Compatível com C++20
- [ ] Compatível com MSVC 2019+
- [ ] Compatível com GCC 11+
- [ ] Compatível com Clang 14+
- [ ] Compatível com Windows 10+
- [ ] Compatível com Ubuntu 20.04+
- [ ] Compatível com macOS Big Sur+

## 📸 Screenshots/Logs

<!-- Se aplicável, adicione screenshots ou logs relevantes -->

## 🤝 Revisão

### Para Revisores

<!-- Informações específicas para quem vai revisar o PR -->

- Foque especialmente em: [área específica]
- Pontos de atenção: [pontos específicos]
- Testes críticos: [testes importantes]

### Perguntas para Discussão

<!-- Se você tem dúvidas ou quer discutir algo específico -->

1. Pergunta sobre implementação X
2. Dúvida sobre abordagem Y
3. Discussão sobre design Z

---

**Nota**: Este PR está pronto para revisão quando todos os itens do checklist estiverem marcados e todos os checks do CI estiverem passando.