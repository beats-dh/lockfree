# Recursos do Projeto LockFree

Este diretório contém os recursos visuais e arquivos de configuração para o executável do projeto LockFree Object Pool.

## Arquivos Incluídos

### Ícone do Aplicativo
- **`icon.ico`** - Ícone principal do executável (32x32 pixels)
- **`icon.svg`** - Versão vetorial do ícone para edição
- **`create_icon.py`** - Script Python para gerar o arquivo ICO

### Recursos do Windows
- **`app.rc`** - Arquivo de recurso do Windows que inclui o ícone no executável

## Design do Ícone

O ícone foi projetado para representar visualmente os conceitos do LockFree Object Pool:

- **Círculo central amarelo** (#fbbf24) - Representa o núcleo do pool de objetos
- **Anéis azuis** (#2563eb, #1e40af) - Representam as operações lock-free e a escalabilidade
- **Nós verdes** (#10b981) - Representam os objetos atômicos e operações concorrentes
- **Linhas de velocidade** - Indicam alta performance

## Como o Ícone é Aplicado

O ícone é automaticamente incluído no executável durante a compilação através do:

1. **Arquivo de recurso** (`app.rc`) que referencia o ícone
2. **Configuração do CMake** que inclui o arquivo de recurso apenas no Windows
3. **Compilação automática** do recurso durante o build

## Personalização

Para modificar o ícone:

1. Edite o arquivo `icon.svg` com qualquer editor de SVG
2. Execute `python create_icon.py` para gerar um novo `icon.ico`
3. Ou use ferramentas online como:
   - [CloudConvert](https://cloudconvert.com/svg-to-ico)
   - [Convertio](https://convertio.co/svg-ico/)
   - [svg2ico.com](https://svg2ico.com/)

## Verificação

Após a compilação, você pode verificar se o ícone foi aplicado:

1. Navegue até `build/bin/lockfree_benchmark.exe`
2. O executável deve exibir o ícone personalizado no Windows Explorer
3. O ícone também aparecerá na barra de tarefas quando o programa estiver em execução

## Compatibilidade

- **Windows**: Totalmente suportado através de arquivos .rc
- **Linux**: Ícones podem ser aplicados através de arquivos .desktop
- **macOS**: Requer configuração específica no bundle da aplicação

Atualmente, o ícone está configurado apenas para Windows. Para outras plataformas, consulte a documentação específica do sistema operacional.