# Ciborgue para Android

Controle local do ESP32-S3 por HTTP na porta 80. O app usa Expo SDK 57, TypeScript e Expo Router.

## Instalação

Tenha Node.js 22.13 ou posterior, JDK 17 e Android SDK 36 para gerar um APK localmente. Ative a depuração USB no celular ou inicie um emulador Android para instalar a build de desenvolvimento.

```sh
cd ciborgue-app
npm ci
npm run type-check
npm run lint
npm test
npm run build
npm run android
```

`npm run build` exporta o bundle Android. `npm run android` compila o APK nativo e o instala. Em sessões seguintes, use `npm start` para abrir o Metro com a build de desenvolvimento instalada. Use `npx expo prebuild --platform android` quando precisar regenerar a configuração nativa. A opção `usesCleartextTraffic` habilita as chamadas HTTP do ESP32 no Android.

## Conexão

1. Conecte o celular à mesma rede Wi-Fi do Ciborgue. Redes de convidados podem isolar os dispositivos.
2. Abra **Ajustes** e toque em **Testar e salvar**. O padrão é `ciborgue.local`.
3. Se o Android não resolver o nome, use o IPv4 exibido pelo Ciborgue ou pelo roteador. O teste usa `GET /api/status` e salva o endereço confirmado.
4. Para configurar pelo portal, conecte-se ao Wi-Fi **Ciborgue-Setup** e acesse `http://192.168.4.1`. Retorne à rede normal e teste o novo endereço quando o robô se conectar.

É possível definir outro endereço inicial em `.env` com `EXPO_PUBLIC_CIBORGUE_HOST`. O app guarda o host no AsyncStorage. Senha Wi-Fi e chave Gemini ficam em memória durante a edição e são enviadas diretamente ao firmware, que as grava em NVS. O histórico do chat fica na memória da sessão. Use uma rede de confiança porque HTTP permite observar o conteúdo das requisições. O chat usa a chave configurada no robô e pode responder pelo mecanismo offline do firmware.

**Início** reúne os controles rápidos e os painéis de expressões, LED, áudio e sistema. **Rotina** oferece presets de timer e Pomodoro, alarmes e acesso a tarefas e memórias. Em **Ajustes**, a seção **Aparência** salva a preferência de tema neste celular.

## API

### Bluetooth

Em **Ajustes › Bluetooth**, busque pelo Ciborgue, selecione a placa e aceite o pareamento na janela do Android. Se o sistema pedir um PIN, consulte os seis dígitos atuais pela serial com `CMD:BLE_PIN`. Em seguida, ajuste brilho, volume, mudo e chirps e toque em **Salvar no robô**. O app confirma os valores com uma nova leitura do GATT. O nível de brilho 0 apaga a luz de fundo.

O Bluetooth requer um cliente de desenvolvimento Expo com o módulo nativo `react-native-ble-plx`. Atualize o cliente instalado antes de abrir esta tela. O Windows pode continuar servindo o bundle com `npm start`. Android 12 ou posterior pede acesso a dispositivos próximos. Nas versões anteriores, a busca pode exigir localização habilitada. O Bluetooth funciona sem Wi-Fi para esses quatro ajustes.

### Wi-Fi

| Método | Caminho | Uso |
| --- | --- | --- |
| GET | /api/status | Telemetria e teste da conexão |
| GET, POST | /api/display | Leitura e ajustes de rotação, inversão e brilho |
| POST | /api/chat | Conversar com o robô |
| POST | /api/memory_proposal | Guardar ou dispensar a proposta identificada no chat |
| POST | /api/clear_history | Limpar o histórico do assistente |
| POST | /api/state, /api/emotion | Alterar o rosto |
| POST | /api/led | Aplicar RGB, cor nomeada ou modo automático |
| POST | /api/audio | Volume, mudo, chirps e efeitos |
| POST | /api/timer | Criar timer de 1 a 86.400 segundos |
| POST | /api/pomodoro | Iniciar ou parar sessão |
| GET, POST | /api/alarms | Listar, criar, ativar, desativar, excluir, silenciar e limpar |
| GET, POST | /api/tasks | Listar, adicionar, concluir, excluir e limpar |
| GET, POST | /api/memories | Listar, adicionar, remover e limpar |
| GET, POST | /api/personality | Consultar personalidade ou exibir card |
| POST | /api/pet, /api/desk_clock | Carinho e relógio de mesa |
| POST | /api/config | Cidade, credenciais e fast path |
| POST | /api/sleep, /api/portal | Energia e portal cativo |
| POST | /api/command | Comandos avançados e cancelamento do timer |

## Comportamento do firmware

- A revisão 3 anuncia as expressões disponíveis e os recursos de tela, sincronização labial e propostas de memória. Placas anteriores continuam com as expressões do conjunto original. Rotação e inversão valem durante a sessão. O firmware atual salva o brilho e informa separadamente o valor efetivo em cada modo de energia.
- Ações de propostas de memória identificam a proposta exibida no chat. O firmware recusa propostas vencidas ou já resolvidas e mantém as frases de confirmação dos clientes antigos.

- A seleção de `BOOT` muda o estado visual. O app usa a extensão de `POST /api/state` deste repositório para isso.
- A restauração automática usa `POST /api/led` com `mode: "STATE"`. A remoção de uma memória usa `POST /api/memories` com `action: "remover"` e o fato exato. Essas funções e o estado `BOOT` são habilitados quando `GET /api/status` anuncia `api_revision: 2`.
- O firmware aceita `limpar` para tarefas. Concluir altera o estado de pendente para concluída. A API atual não oferece renomear ou reabrir tarefas.
- Depois que o ESP32 entra em sono leve, o despertar depende de toque ou botão se o Wi-Fi parar. **Acordar** por HTTP funciona enquanto o servidor estiver acessível.
- O estado visual `PORTAL` e a abertura real do ponto de acesso são operações distintas.
- Avançado mostra o status retornado pela API. Respostas seriais como `PONG` e o relatório `AP_DEBUG` ficam na serial da placa.
- O timer dispara som no robô. O app sinaliza visualmente quando observa o fim do timer durante o estado de alerta.
- A versão web é experimental. O navegador pode exigir permissão de rede local e bloquear HTTP quando o app web for servido por HTTPS. Android é a plataforma de referência.
