# UX & UI Improvements Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the 5 prioritized UX/UI improvements for Ciborgue App: categorizing Settings, adding Dock state badges, horizontal quick-reply chips in Chat, native edge-swipe back gestures, and haptic/microinteraction polish for Pet and Expressions.

**Architecture:** 
- Modular React Native UI components conforming to theme tokens and accessibility roles.
- Settings categorizes cards into Network, Device, and System tabs to eliminate cognitive overload and scroll fatigue.
- Dock tabbar consumes telemetry store to render reactive status badges (active timer/pomo/ringing).
- Chat replaces wrapping action rows with an accessible horizontal chip scrollview and animated typing state.
- Screen component incorporates edge-swipe gesture responder delegating to `onBack` with haptics.
- Microinteractions integrate `expo-haptics` and animated feedback for affectionate and emotional robot controls.

**Tech Stack:** React Native (0.86), Expo Router (57), TypeScript, Vitest, React Test Renderer, `expo-haptics`, `@expo/vector-icons`.

**Spec:** UX Review & Prioritization from Session `6b20bb78-1474-4d13-ba3a-e53457098d6e` (Categories in Settings, Dock Badges, Chat Prompt Chips, Swipe-to-Back, Pet/Expression Microinteractions).

## Global Constraints

- Must maintain 100% test pass rate across node tests (`protocol.test.ts`) and Vitest (`components.spec.tsx`).
- Must pass `npm run type-check` and `npm run lint`.
- All interactive controls must retain accessibility roles, labels, and reduced-motion compatibility.
- Use existing theme palette (`c.accent`, `c.tint`, `c.surface`, `c.line`, `c.text`, `c.muted`) without hardcoded hex colors where theme colors apply.
- Must execute Node/npm commands with TLS environment variables: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem`.

## Review Focus

- Empty or undefined telemetry must not break Dock badge rendering.
- Category filter in Settings must preserve all 8 existing configuration cards and allow switching without resetting local input draft state.
- Chat prompt chips must scroll horizontally without obstructing virtual keyboard or submit button.
- Edge-swipe gesture must only trigger when gesture starts near the screen edge (`x <= 40`), preventing conflicts with internal horizontal sliders or scroll views.
- Haptics calls must be safely wrapped or ignored gracefully when running in environments where haptics are unsupported or disabled.

---

### Task 1: Reorganize Settings into Clean Categories

**Files:**
- Modify: `ciborgue-app/src/app/configuracoes.tsx`
- Test: `ciborgue-app/tests/components.spec.tsx`

**Interfaces:**
- Produces: `SettingsCategory = 'todas' | 'rede' | 'dispositivo' | 'sistema'` state and segmented category navigation.
- Consumes: `Choices` or segmented chips from `components/ui.tsx`, existing cards and forms.

- [ ] **Step 1: Write test for Settings category filtering**

Add a test in `tests/components.spec.tsx` that verifies Settings renders category selector and filters cards according to the active category ('Rede', 'Dispositivo', 'Sistema').

- [ ] **Step 2: Run test to verify it fails**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: FAIL (category filter not yet present in Settings).

- [ ] **Step 3: Implement categorized sections in `src/app/configuracoes.tsx`**

Add segmented category selector at the top of `Settings`:
- `'rede'`: Endereço do Ciborgue, Wi-Fi do robô, Portal cativo, Bluetooth.
- `'dispositivo'`: Tela, Áudio, Preferências do robô.
- `'sistema'`: Aparência, Manutenção (Avançado).
- Provide filter chips allowing the user to select specific category or view all.

- [ ] **Step 4: Run test to verify it passes**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add ciborgue-app/src/app/configuracoes.tsx ciborgue-app/tests/components.spec.tsx
git commit -m "feat(settings): categorize settings cards into network, device, and system"
```

---

### Task 2: Reactive State Badges in Dock

**Files:**
- Modify: `ciborgue-app/src/components/dock.tsx`
- Test: `ciborgue-app/tests/components.spec.tsx`

**Interfaces:**
- Produces: Dock badge indicator rendered on tabs when telemetry indicates background activity (e.g. active timer/pomodoro on 'tempo' tab).
- Consumes: `useTelemetry()` from `../stores/robot`.

- [ ] **Step 1: Write test for Dock activity badge**

Add test in `tests/components.spec.tsx` verifying that when snapshot data contains an active timer (`timerTotal > 0` or `pomo`), the Rotina tab displays a badge indicator with accessibility description.

- [ ] **Step 2: Run test to verify it fails**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: FAIL.

- [ ] **Step 3: Implement badge in `src/components/dock.tsx`**

Connect `useTelemetry()` in `Dock`:
- If `s?.timerTotal || s?.pomo || s?.ringing`, show a small badge dot on the `tempo` tab icon.
- Provide proper accessibility label on the tab item indicating active activity (e.g. "Rotina (Timer ativo)").

- [ ] **Step 4: Run test to verify it passes**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add ciborgue-app/src/components/dock.tsx ciborgue-app/tests/components.spec.tsx
git commit -m "feat(dock): add reactive activity badge to navigation dock"
```

---

### Task 3: Horizontal Scrollable Chips and Typing Indicator in Chat

**Files:**
- Modify: `ciborgue-app/src/app/chat.tsx`
- Test: `ciborgue-app/tests/components.spec.tsx`

**Interfaces:**
- Consumes: `quick` array of prompt suggestions, `setSending`, `setText`.
- Produces: Horizontal scrolling chip bar for quick questions with accessibility roles and haptics, plus animated typing indicator.

- [ ] **Step 1: Write test for Chat quick prompt chips**

Add a test in `tests/components.spec.tsx` verifying quick prompt suggestions are rendered as accessible chips in a horizontal scrollable view and tapping one updates the message field.

- [ ] **Step 2: Run test to verify it fails**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: FAIL.

- [ ] **Step 3: Implement horizontal chips and refined typing in `src/app/chat.tsx`**

- Replace `<Row>{quick.map(...)}</Row>` with an accessible `ScrollView` (`horizontal showsHorizontalScrollIndicator={false}`) containing styled chip pills with an icon.
- Wrap quick chip selection with subtle haptic feedback.
- Render styled typing dots / animated state when `sending` is true.

- [ ] **Step 4: Run test to verify it passes**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add ciborgue-app/src/app/chat.tsx ciborgue-app/tests/components.spec.tsx
git commit -m "feat(chat): add horizontal quick prompt chips and enhanced typing indicator"
```

---

### Task 4: Edge-Swipe Back Gesture in Secondary Screens

**Files:**
- Modify: `ciborgue-app/src/components/ui.tsx`
- Test: `ciborgue-app/tests/components.spec.tsx`

**Interfaces:**
- Consumes: `onBack` prop of `Screen` component.
- Produces: `PanResponder` attached to `Screen` that detects a rightward swipe starting near the left edge (`x <= 40`, `dx >= 60`, `|dy| < 40`) and executes `onBack()`.

- [ ] **Step 1: Write test for Screen edge swipe-to-back**

Add test in `tests/components.spec.tsx` verifying that simulated PanResponder swipe starting at edge (`x0: 20, dx: 80`) invokes `onBack`.

- [ ] **Step 2: Run test to verify it fails**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: FAIL.

- [ ] **Step 3: Implement edge-swipe PanResponder in `Screen` in `src/components/ui.tsx`**

- When `onBack` is provided, initialize a lightweight `PanResponder`:
  - `onMoveShouldSetPanResponder`: true only if `gestureState.x0 <= 40 && gestureState.dx > 25 && Math.abs(gestureState.dy) < 30`.
  - `onPanResponderRelease`: if `gestureState.dx > 60`, trigger `onBack()` and fire `Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Light)`.
- Apply `panHandlers` to the screen root container.

- [ ] **Step 4: Run test to verify it passes**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add ciborgue-app/src/components/ui.tsx ciborgue-app/tests/components.spec.tsx
git commit -m "feat(ui): add native edge-swipe back gesture to Screen"
```

---

### Task 5: Microinteractions & Delight for Carinho and Expressions

**Files:**
- Modify: `ciborgue-app/src/app/index.tsx`
- Modify: `ciborgue-app/src/components/robot-controls.tsx`
- Test: `ciborgue-app/tests/components.spec.tsx`

**Interfaces:**
- Consumes: `expo-haptics`, `Animated`, `send('pet')`.
- Produces: Heart pulse microanimation on Carinho button press + haptic feedback on Pet and Expression selection.

- [ ] **Step 1: Write test for Pet interaction**

Add test in `tests/components.spec.tsx` verifying that pressing Carinho triggers pet action and renders active feedback state.

- [ ] **Step 2: Run test to verify it fails**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npx vitest run tests/components.spec.tsx`
Expected: FAIL.

- [ ] **Step 3: Implement microinteractions in `index.tsx` and `robot-controls.tsx`**

- In `Shortcut` / `Dashboard`:
  - When Pet is pressed, trigger `Haptics.impactAsync(Haptics.ImpactFeedbackStyle.Medium)`.
  - Add an animated scale pulse on the heart icon.
- In `ExpressionControls`:
  - When state/emotion choice is tapped, trigger `Haptics.selectionAsync()`.

- [ ] **Step 4: Run test and full suite to verify it passes**

Run: `export OPENSSL_CONF=/data/user/0/id.vern.antigravity/files/usr/etc/tls/openssl.cnf && export SSL_CERT_FILE=/data/user/0/id.vern.antigravity/files/usr/etc/tls/cert.pem && cd /data/user/0/id.vern.antigravity/files/home/ciborgue-esp32/ciborgue-app && npm run type-check && npm run lint && npm test`
Expected: PASS with 0 lint errors, 0 type errors, all tests passing.

- [ ] **Step 5: Commit**

```bash
git add ciborgue-app/src/app/index.tsx ciborgue-app/src/components/robot-controls.tsx ciborgue-app/tests/components.spec.tsx
git commit -m "feat(interactions): add haptics and pulse animation for pet and expressions"
```
