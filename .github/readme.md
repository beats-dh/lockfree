# GitHub Actions e Automação

Este diretório contém toda a configuração de automação e CI/CD para o projeto LockFree Object Pool.

## 📁 Estrutura

```
.github/
├── workflows/           # Workflows do GitHub Actions
│   ├── ci.yml          # Pipeline principal de CI/CD
│   ├── benchmark.yml   # Benchmarks de performance
│   ├── security.yml    # Verificações de segurança
│   └── docs.yml        # Geração de documentação
├── ISSUE_TEMPLATE/     # Templates para issues
│   ├── bug_report.yml  # Template para bugs
│   └── feature_request.yml # Template para features
├── dependabot.yml      # Configuração do Dependabot
├── pull_request_template.md # Template para PRs
└── README.md           # Este arquivo
```

## 🚀 Workflows

### 1. CI/CD Pipeline (`ci.yml`)

**Triggers:**
- Push para `main` e `develop`
- Pull requests para `main`
- Releases publicados

**Jobs:**
- **format-check**: Verificação de formatação com clang-format
- **build-windows**: Build e teste no Windows (Debug/Release, x64)
- **build-linux**: Build e teste no Linux (GCC 11, Clang 14)
- **build-macos**: Build e teste no macOS
- **codeql-analysis**: Análise de segurança com CodeQL
- **release**: Criação automática de releases com artifacts

**Recursos:**
- Cache do vcpkg para builds mais rápidos
- Testes automatizados com CTest
- Upload de artifacts para releases
- Suporte a múltiplos compiladores e plataformas

### 2. Performance Benchmarks (`benchmark.yml`)

**Triggers:**
- Agendado: Segundas-feiras às 02:00 UTC
- Execução manual com parâmetros customizáveis

**Funcionalidades:**
- Benchmarks categorizados (Pool, Stress, Multithread)
- Resultados em formato JSON
- Relatórios consolidados multi-plataforma
- Comentários automáticos em PRs
- Retenção de resultados por 90 dias

**Parâmetros Configuráveis:**
- `benchmark_filter`: Filtro para benchmarks específicos
- `iterations`: Número de iterações (padrão: 10)

### 3. Security & Dependencies (`security.yml`)

**Triggers:**
- Push para `main` e `develop`
- Pull requests para `main`
- Agendado: Segundas-feiras às 03:00 UTC

**Verificações:**
- **dependency-check**: Análise de dependências vulneráveis
- **static-analysis**: Análise estática com Clang e cppcheck
- **license-check**: Verificação de conformidade de licenças
- **semgrep-analysis**: Análise de segurança com Semgrep

### 4. Documentation (`docs.yml`)

**Triggers:**
- Mudanças em arquivos de documentação
- Mudanças no código fonte
- Execução manual

**Funcionalidades:**
- **docs-check**: Verificação de completude da documentação
- **generate-doxygen**: Geração automática de documentação API
- **link-check**: Verificação de links quebrados
- **code-metrics**: Métricas de código com cloc
- Deploy automático para GitHub Pages

## 🤖 Dependabot

Configuração automática para manter dependências atualizadas:

- **GitHub Actions**: Atualizações semanais às segundas-feiras
- **vcpkg**: Monitoramento manual (vcpkg não é suportado nativamente)
- Labels automáticos e assignees configuráveis
- Limite de 5 PRs abertos simultaneamente

## 📝 Templates

### Issue Templates

1. **Bug Report** (`bug_report.yml`):
   - Formulário estruturado para reportar bugs
   - Campos obrigatórios para reprodução
   - Informações de ambiente e sistema
   - Labels automáticos: `bug`, `needs-triage`

2. **Feature Request** (`feature_request.yml`):
   - Formulário para sugestões de funcionalidades
   - Categorização e priorização
   - Exemplos de uso e implementação
   - Labels automáticos: `enhancement`, `needs-discussion`

### Pull Request Template

- Checklist completo para qualidade de código
- Seções para testes e performance
- Verificação de breaking changes
- Guias de migração quando necessário

## 🔧 Configuração

### Secrets Necessários

Para funcionalidade completa, configure os seguintes secrets no repositório:

```bash
# Opcional: Para análise avançada com Semgrep
SEMGREP_APP_TOKEN=your_semgrep_token

# Automático: GitHub fornece automaticamente
GITHUB_TOKEN=automatic
```

### Variáveis de Ambiente

```yaml
# Cache do vcpkg para builds mais rápidos
VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"
```

### Permissões Necessárias

```yaml
# Para workflows que fazem deploy ou análise de segurança
permissions:
  contents: write          # Para releases e GitHub Pages
  security-events: write   # Para CodeQL e SARIF uploads
  actions: read           # Para cache do GitHub Actions
```

## 📊 Monitoramento

### Métricas Coletadas

1. **Performance**:
   - Tempo de execução dos benchmarks
   - Throughput e latência
   - Uso de memória

2. **Qualidade**:
   - Cobertura de testes
   - Warnings de compilação
   - Análise estática

3. **Segurança**:
   - Vulnerabilidades em dependências
   - Problemas de segurança no código
   - Conformidade de licenças

### Relatórios Gerados

- **Benchmark Reports**: Resultados de performance multi-plataforma
- **Security Summary**: Consolidação de todas as verificações de segurança
- **Documentation Summary**: Status da documentação e métricas de código
- **Dependency Reports**: Lista de dependências e suas licenças

## 🚨 Troubleshooting

### Problemas Comuns

1. **Build Failures**:
   - Verificar se vcpkg está configurado corretamente
   - Confirmar que todas as dependências estão disponíveis
   - Verificar compatibilidade do compilador

2. **Cache Issues**:
   - Limpar cache do GitHub Actions se necessário
   - Verificar configuração do `VCPKG_BINARY_SOURCES`

3. **Permission Errors**:
   - Verificar se o repositório tem as permissões necessárias
   - Confirmar configuração dos secrets

### Logs e Debugging

- Todos os workflows geram logs detalhados
- Artifacts são mantidos por 30-90 dias
- Use `workflow_dispatch` para execução manual com debugging

## 🔄 Manutenção

### Atualizações Regulares

1. **Mensal**:
   - Revisar e atualizar versões do vcpkg
   - Verificar novas versões das GitHub Actions
   - Atualizar dependências de desenvolvimento

2. **Trimestral**:
   - Revisar e otimizar workflows
   - Atualizar templates baseado em feedback
   - Verificar métricas de performance dos workflows

3. **Anual**:
   - Revisar estratégia completa de CI/CD
   - Avaliar novas ferramentas e práticas
   - Atualizar documentação e guias

### Customização

Para adaptar os workflows às suas necessidades:

1. **Modificar Triggers**: Ajustar quando os workflows executam
2. **Adicionar Plataformas**: Incluir novos sistemas operacionais
3. **Configurar Notificações**: Adicionar integração com Slack/Discord
4. **Personalizar Relatórios**: Modificar formato e conteúdo dos relatórios

---

**Nota**: Esta configuração foi projetada para projetos C++ de alta performance com foco em qualidade, segurança e documentação. Adapte conforme necessário para seu caso de uso específico.
