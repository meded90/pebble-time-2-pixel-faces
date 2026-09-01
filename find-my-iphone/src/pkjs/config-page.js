'use strict';

var COPY = {
  en: {
    subtitle: 'Pebble companion settings', stored: 'Stored on iPhone:', storedValue: 'Apple session tokens, cookies, and the selected iPhone ID.', notStored: 'Not stored:', notStoredValue: 'password or 2FA code.',
    step1: 'STEP 1 OF 3', connect: 'Connect Apple Account', loginInfo: 'Sign-in goes directly to Apple servers. This unofficial reverse API may change.', account: 'Apple Account', password: 'Password', continueLabel: 'Continue',
    step2: 'STEP 2 OF 3', verification: 'Verification code', codeInfo: 'Enter the six-digit code shown on a trusted Apple device.', code: '2FA code', verify: 'Verify code', restart: 'Start again',
    step3: 'STEP 3 OF 3', which: 'Which iPhone should ring?', deviceInfo: 'Use Up and Down on the watch to switch devices.', save: 'Save', refresh: 'Refresh list',
    ready: 'Ready to find', selected: '{device} is selected. Hold Select on Pebble for 0.65 seconds.', session: 'Apple session', active: 'Active', notSaved: 'Not saved', change: 'Change device', logout: 'Sign out and delete session'
  },
  ru: {
    subtitle: 'Настройки приложения Pebble', stored: 'Хранится на iPhone:', storedValue: 'токены Apple-сессии, cookies и ID выбранного iPhone.', notStored: 'Не хранится:', notStoredValue: 'пароль и код 2FA.',
    step1: 'ШАГ 1 ИЗ 3', connect: 'Подключить Apple Account', loginInfo: 'Вход выполняется напрямую через серверы Apple. Это неофициальный reverse API, который Apple может изменить.', account: 'Apple Account', password: 'Пароль', continueLabel: 'Продолжить',
    step2: 'ШАГ 2 ИЗ 3', verification: 'Код проверки', codeInfo: 'Введите шестизначный код с доверенного устройства Apple.', code: 'Код 2FA', verify: 'Проверить код', restart: 'Начать заново',
    step3: 'ШАГ 3 ИЗ 3', which: 'Какой iPhone звонит?', deviceInfo: 'Up и Down на часах переключают устройства.', save: 'Сохранить', refresh: 'Обновить список',
    ready: 'Готово к поиску', selected: 'Выбран {device}. На Pebble удерживайте Select 0,65 секунды.', session: 'Сессия Apple', active: 'Активна', notSaved: 'Не сохранён', change: 'Сменить устройство', logout: 'Выйти и удалить сессию'
  },
  uk: {
    subtitle: 'Налаштування Pebble', stored: 'Зберігається на iPhone:', storedValue: 'токени сесії Apple, cookies та ID вибраного iPhone.', notStored: 'Не зберігається:', notStoredValue: 'пароль і код 2FA.',
    step1: 'КРОК 1 З 3', connect: 'Підключити Apple Account', loginInfo: 'Вхід виконується безпосередньо через сервери Apple. Цей неофіційний reverse API може змінитися.', password: 'Пароль', continueLabel: 'Продовжити',
    step2: 'КРОК 2 З 3', verification: 'Код перевірки', codeInfo: 'Введіть шестизначний код із довіреного пристрою Apple.', code: 'Код 2FA', verify: 'Перевірити код', restart: 'Почати знову',
    step3: 'КРОК 3 З 3', which: 'Який iPhone має дзвонити?', deviceInfo: 'Up і Down на годиннику перемикають пристрої.', save: 'Зберегти', refresh: 'Оновити список',
    ready: 'Готово до пошуку', selected: 'Вибрано {device}. Утримуйте Select на Pebble 0,65 секунди.', session: 'Сесія Apple', active: 'Активна', notSaved: 'Не збережено', change: 'Змінити пристрій', logout: 'Вийти й видалити сесію'
  },
  de: {
    subtitle: 'Pebble-Begleiteinstellungen', stored: 'Auf dem iPhone gespeichert:', storedValue: 'Apple-Sitzungstoken, Cookies und die gewählte iPhone-ID.', notStored: 'Nicht gespeichert:', notStoredValue: 'Passwort oder 2FA-Code.',
    step1: 'SCHRITT 1 VON 3', connect: 'Apple Account verbinden', loginInfo: 'Die Anmeldung erfolgt direkt bei Apple. Diese inoffizielle Reverse API kann sich ändern.', password: 'Passwort', continueLabel: 'Weiter',
    step2: 'SCHRITT 2 VON 3', verification: 'Bestätigungscode', codeInfo: 'Gib den sechsstelligen Code eines vertrauenswürdigen Apple-Geräts ein.', code: '2FA-Code', verify: 'Code prüfen', restart: 'Neu beginnen',
    step3: 'SCHRITT 3 VON 3', which: 'Welches iPhone soll klingeln?', deviceInfo: 'Mit Up und Down auf der Uhr wechselst du das Gerät.', save: 'Speichern', refresh: 'Liste aktualisieren',
    ready: 'Bereit zum Finden', selected: '{device} ist ausgewählt. Halte Select auf Pebble 0,65 Sekunden.', session: 'Apple-Sitzung', active: 'Aktiv', notSaved: 'Nicht gespeichert', change: 'Gerät wechseln', logout: 'Abmelden und Sitzung löschen'
  },
  es: {
    subtitle: 'Ajustes complementarios de Pebble', stored: 'Guardado en el iPhone:', storedValue: 'tokens de sesión de Apple, cookies e ID del iPhone elegido.', notStored: 'No se guarda:', notStoredValue: 'contraseña ni código 2FA.',
    step1: 'PASO 1 DE 3', connect: 'Conectar Apple Account', loginInfo: 'El inicio de sesión va directamente a Apple. Esta API inversa no oficial puede cambiar.', password: 'Contraseña', continueLabel: 'Continuar',
    step2: 'PASO 2 DE 3', verification: 'Código de verificación', codeInfo: 'Introduce el código de seis dígitos de un dispositivo Apple de confianza.', code: 'Código 2FA', verify: 'Verificar código', restart: 'Empezar de nuevo',
    step3: 'PASO 3 DE 3', which: '¿Qué iPhone debe sonar?', deviceInfo: 'Usa Up y Down en el reloj para cambiar de dispositivo.', save: 'Guardar', refresh: 'Actualizar lista',
    ready: 'Listo para buscar', selected: '{device} está seleccionado. Mantén Select en Pebble durante 0,65 segundos.', session: 'Sesión de Apple', active: 'Activa', notSaved: 'No guardada', change: 'Cambiar dispositivo', logout: 'Cerrar sesión y eliminarla'
  },
  fr: {
    subtitle: 'Réglages compagnon Pebble', stored: 'Stocké sur l’iPhone :', storedValue: 'jetons de session Apple, cookies et ID de l’iPhone choisi.', notStored: 'Non stocké :', notStoredValue: 'mot de passe ou code 2FA.',
    step1: 'ÉTAPE 1 SUR 3', connect: 'Connecter Apple Account', loginInfo: 'La connexion passe directement par Apple. Cette API inverse non officielle peut changer.', password: 'Mot de passe', continueLabel: 'Continuer',
    step2: 'ÉTAPE 2 SUR 3', verification: 'Code de vérification', codeInfo: 'Saisissez le code à six chiffres d’un appareil Apple de confiance.', code: 'Code 2FA', verify: 'Vérifier le code', restart: 'Recommencer',
    step3: 'ÉTAPE 3 SUR 3', which: 'Quel iPhone doit sonner ?', deviceInfo: 'Utilisez Up et Down sur la montre pour changer d’appareil.', save: 'Enregistrer', refresh: 'Actualiser la liste',
    ready: 'Prêt à localiser', selected: '{device} est sélectionné. Maintenez Select sur Pebble pendant 0,65 seconde.', session: 'Session Apple', active: 'Active', notSaved: 'Non enregistré', change: 'Changer d’appareil', logout: 'Se déconnecter et supprimer la session'
  },
  it: {
    subtitle: 'Impostazioni companion Pebble', stored: 'Memorizzato su iPhone:', storedValue: 'token della sessione Apple, cookie e ID dell’iPhone scelto.', notStored: 'Non memorizzato:', notStoredValue: 'password o codice 2FA.',
    step1: 'PASSAGGIO 1 DI 3', connect: 'Collega Apple Account', loginInfo: 'L’accesso avviene direttamente con Apple. Questa API inversa non ufficiale può cambiare.', password: 'Password', continueLabel: 'Continua',
    step2: 'PASSAGGIO 2 DI 3', verification: 'Codice di verifica', codeInfo: 'Inserisci il codice a sei cifre da un dispositivo Apple attendibile.', code: 'Codice 2FA', verify: 'Verifica codice', restart: 'Ricomincia',
    step3: 'PASSAGGIO 3 DI 3', which: 'Quale iPhone deve suonare?', deviceInfo: 'Usa Up e Down sull’orologio per cambiare dispositivo.', save: 'Salva', refresh: 'Aggiorna elenco',
    ready: 'Pronto per la ricerca', selected: '{device} è selezionato. Tieni Select su Pebble per 0,65 secondi.', session: 'Sessione Apple', active: 'Attiva', notSaved: 'Non salvata', change: 'Cambia dispositivo', logout: 'Esci ed elimina la sessione'
  },
  pt: {
    subtitle: 'Definições complementares Pebble', stored: 'Guardado no iPhone:', storedValue: 'tokens da sessão Apple, cookies e ID do iPhone selecionado.', notStored: 'Não é guardado:', notStoredValue: 'palavra-passe ou código 2FA.',
    step1: 'PASSO 1 DE 3', connect: 'Ligar Apple Account', loginInfo: 'O início de sessão é feito diretamente na Apple. Esta API inversa não oficial pode mudar.', password: 'Palavra-passe', continueLabel: 'Continuar',
    step2: 'PASSO 2 DE 3', verification: 'Código de verificação', codeInfo: 'Introduza o código de seis dígitos de um dispositivo Apple fidedigno.', code: 'Código 2FA', verify: 'Verificar código', restart: 'Começar de novo',
    step3: 'PASSO 3 DE 3', which: 'Qual iPhone deve tocar?', deviceInfo: 'Use Up e Down no relógio para mudar de dispositivo.', save: 'Guardar', refresh: 'Atualizar lista',
    ready: 'Pronto para localizar', selected: '{device} está selecionado. Segure Select no Pebble por 0,65 segundos.', session: 'Sessão Apple', active: 'Ativa', notSaved: 'Não guardada', change: 'Mudar dispositivo', logout: 'Terminar sessão e eliminá-la'
  }
};

function escapeHtml(value) {
  return String(value || '').replace(/[&<>"']/g, function(character) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[character];
  });
}

function languageFor(locale) {
  var language = String(locale || '').toLowerCase().split(/[-_]/)[0];
  return COPY[language] ? language : 'en';
}

function copyFor(model) {
  var locale = model.locale;
  if (!locale && typeof navigator !== 'undefined') locale = navigator.language;
  var language = languageFor(locale);
  return { language: language, text: Object.assign({}, COPY.en, COPY[language]) };
}

function privacy(copy) {
  return '<div class="privacy"><b>' + copy.stored + '</b> ' + copy.storedValue + '<br><b>' + copy.notStored + '</b> ' + copy.notStoredValue + '</div>';
}

function pageBody(model, copy) {
  var stage = model.stage || 'login';
  var error = model.error ? '<div class="error">' + escapeHtml(model.error) + '</div>' : '';
  if (stage === 'two-factor') {
    return '<p class="step">' + copy.step2 + '</p><h1>' + copy.verification + '</h1>' + error + '<p>' + copy.codeInfo + '</p>' +
      '<form id="form"><label>' + copy.code + '<input name="code" inputmode="numeric" pattern="[0-9]{6}" maxlength="6" autocomplete="one-time-code" required></label>' +
      '<button>' + copy.verify + '</button></form><button class="secondary" data-action="logout">' + copy.restart + '</button>' + privacy(copy);
  }
  if (stage === 'devices') {
    var devices = model.devices || [];
    var options = devices.map(function(device, index) {
      var checked = device.id === model.selectedDeviceId || (!model.selectedDeviceId && index === 0) ? ' checked' : '';
      return '<label class="device"><input type="radio" name="deviceId" value="' + escapeHtml(device.id) + '"' + checked + '><span>▯</span><b>' + escapeHtml(device.name) + '</b><i>✓</i></label>';
    }).join('');
    return '<p class="step">' + copy.step3 + '</p><h1>' + copy.which + '</h1>' + error + '<p>' + copy.deviceInfo + '</p><form id="form">' + options +
      '<button' + (devices.length ? '' : ' disabled') + '>' + copy.save + '</button></form><button class="secondary" data-action="refresh">' + copy.refresh + '</button>' + privacy(copy);
  }
  if (stage === 'connected') {
    var deviceName = escapeHtml(model.selectedDeviceName || 'iPhone');
    return '<div class="success">✓</div><h1>' + copy.ready + '</h1>' + error + '<p><b>' + copy.selected.replace('{device}', deviceName) + '</b></p>' +
      '<div class="session"><span>' + copy.session + '</span><b>' + copy.active + '</b><span>' + copy.password + '</span><b>' + copy.notSaved + '</b></div>' +
      '<button data-action="devices">' + copy.change + '</button><button class="danger" data-action="logout">' + copy.logout + '</button>' + privacy(copy);
  }
  return '<p class="step">' + copy.step1 + '</p><h1>' + copy.connect + '</h1>' + error + '<p>' + copy.loginInfo + '</p>' +
    '<form id="form"><label>' + copy.account + '<input name="appleId" type="email" autocomplete="username" value="' + escapeHtml(model.appleId || '') + '" required></label>' +
    '<label>' + copy.password + '<input name="password" type="password" autocomplete="current-password" required></label><button>' + copy.continueLabel + '</button></form>' + privacy(copy);
}

function generateUrl(model) {
  model = model || {};
  var localized = copyFor(model);
  var copy = localized.text;
  var stage = model.stage || 'login';
  var submitScript = '';
  if (stage === 'login') submitScript = "send({action:'login',appleId:f.appleId.value,password:f.password.value,locale:navigator.language});f.password.value='';";
  else if (stage === 'two-factor') submitScript = "send({action:'verify',code:f.code.value,locale:navigator.language});f.code.value='';";
  else if (stage === 'devices') submitScript = "var d=f.querySelector('input[name=deviceId]:checked');if(d)send({action:'select',deviceId:d.value,locale:navigator.language});";
  var html = '<!doctype html><html lang="' + localized.language + '"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Find My iPhone</title><style>' +
    '*{box-sizing:border-box}body{margin:0;padding:24px 18px 50px;color:#0b1739;background:#f3f6fb;font:15px -apple-system,BlinkMacSystemFont,sans-serif}main{max-width:480px;margin:auto;padding:25px;border:1px solid #dce2ee;border-radius:22px;background:white;box-shadow:0 18px 55px #18305a1f}.brand{display:flex;gap:10px;align-items:center;padding-bottom:18px;border-bottom:1px solid #e1e6ef}.icon{display:grid;width:42px;height:42px;place-content:center;border-radius:12px;background:#ffc800;font-size:25px;font-weight:900}.brand small{display:block;margin-top:3px;color:#68758b}.step{margin:24px 0 0!important;color:#1466ff!important;font-size:11px!important;font-weight:900;letter-spacing:.12em}h1{margin:7px 0 8px;font-size:28px;letter-spacing:-.04em}p{color:#5e6c82;line-height:1.55}label{display:grid;gap:7px;margin-top:18px;font-size:12px;font-weight:800}input{width:100%;padding:14px;border:1px solid #ccd4e2;border-radius:11px;font:inherit}button{width:100%;margin-top:20px;padding:14px;border:0;border-radius:12px;color:white;background:#1466ff;font:800 15px inherit}button:disabled{opacity:.45}.secondary{margin-top:9px;color:#1466ff;background:#eaf1ff}.danger{margin-top:9px;color:#b32737;background:#fff0f2}.privacy,.session,.error{margin-top:20px;padding:13px;border-radius:12px;font-size:12px;line-height:1.55}.privacy,.session{color:#53627a;background:#f1f4f9}.error{color:#a52635;background:#fff0f2}.device{position:relative;display:grid;grid-template-columns:auto 1fr auto;gap:12px;align-items:center;padding:14px;border:1px solid #dce2ee;border-radius:13px}.device:has(input:checked){border-color:#1466ff;background:#f2f6ff}.device input{position:absolute;opacity:0}.device span{font-size:24px}.device i{display:none;color:#1466ff}.device:has(input:checked) i{display:block}.success{display:grid;width:76px;height:76px;margin:25px auto 18px;place-content:center;border-radius:50%;color:white;background:#12b76a;font-size:34px;font-weight:900}.session{display:grid;grid-template-columns:1fr auto;gap:10px}.session b{text-align:right;color:#0b1739}</style></head><body><main>' +
    '<div class="brand"><span class="icon">⌕</span><div><b>Find My iPhone</b><small>' + copy.subtitle + '</small></div></div>' + pageBody(model, copy) +
    '</main><script>function send(v){location.href="pebblejs://close#"+encodeURIComponent(JSON.stringify(v))}var f=document.getElementById("form");if(f)f.addEventListener("submit",function(e){e.preventDefault();' + submitScript +
    '});Array.prototype.forEach.call(document.querySelectorAll("[data-action]"),function(b){b.onclick=function(){send({action:b.getAttribute("data-action"),locale:navigator.language})}});<\/script></body></html>';
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}

module.exports = { generateUrl: generateUrl, languageFor: languageFor };
