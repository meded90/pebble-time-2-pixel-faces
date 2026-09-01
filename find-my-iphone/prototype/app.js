(function() {
  'use strict';

  var language = /^ru|^uk|^be/i.test(navigator.language || '') ? 'ru' : 'en';
  var text = language === 'ru' ? {
    find: 'Найти iPhone', connecting: 'Подключение…', signIn: 'Нужен вход', settings: 'Откройте настройки',
    hold: 'Удерживайте Select', keep: 'Продолжайте', cancel: 'Отпустите для отмены', contacting: 'Связь с Apple', sending: 'Отправка…',
    sent: 'Сигнал отправлен', rings: 'iPhone зазвонит', offline: 'Телефон не в сети', retry: 'Select — повторить', again: 'Войдите снова',
    wait: 'Подождите 10 сек', later: 'Повторите позже', failed: 'Не удалось'
  } : {
    find: 'Find my iPhone', connecting: 'Connecting…', signIn: 'Sign in required', settings: 'Open phone settings',
    hold: 'Hold Select', keep: 'Keep holding', cancel: 'Release to cancel', contacting: 'Contacting Apple', sending: 'Sending…',
    sent: 'Sound sent', rings: 'iPhone should ring', offline: 'Phone offline', retry: 'Select to retry', again: 'Sign in again',
    wait: 'Wait 10 seconds', later: 'Try again later', failed: 'Could not send'
  };
  document.documentElement.lang = language;
  var states = {
    BOOT: { title: text.find, device: '', hint: text.connecting, wave: 0 },
    AUTH_REQUIRED: { title: text.signIn, device: '', hint: text.settings, wave: 0, badge: 'auth' },
    READY: { title: text.find, device: 'Kirill’s iPhone', hint: text.hold, wave: 0 },
    ARMING: { title: text.keep, device: 'Kirill’s iPhone', hint: text.cancel, wave: 0, progress: true },
    REQUESTING: { title: text.contacting, device: 'Kirill’s iPhone', hint: text.sending, wave: 1 },
    RINGING_ACCEPTED: { title: text.sent, device: 'Kirill’s iPhone', hint: text.rings, wave: 2, badge: 'success' },
    OFFLINE: { title: text.offline, device: '', hint: text.retry, wave: 0, badge: 'offline' },
    AUTH_EXPIRED: { title: text.again, device: '', hint: text.settings, wave: 0, badge: 'auth' },
    RATE_LIMITED: { title: text.wait, device: '', hint: text.later, wave: 0, badge: 'error' },
    ERROR: { title: text.failed, device: 'ERROR 500', hint: text.retry, wave: 0, badge: 'error' }
  };
  var deviceNames = ['Kirill’s iPhone', 'Travel iPhone'];
  var state = 'READY';
  var deviceIndex = 0;
  var holdTimer = null;
  var resultTimer = null;
  var ringTimer = null;

  var phoneSprite = document.getElementById('phone-sprite');
  var cushionSprite = document.getElementById('cushion-sprite');
  var badge = document.getElementById('badge');
  var title = document.getElementById('watch-title');
  var device = document.getElementById('device-name');
  var hint = document.getElementById('watch-hint');
  var holdTrack = document.getElementById('hold-track');
  var holdProgress = document.getElementById('hold-progress');
  var eventLog = document.getElementById('event-log');
  var closedOverlay = document.getElementById('closed-overlay');

  function image(name) {
    var version = /^(scene-foreground|wave-)/.test(name) ? 'v4' : 'v3';
    return '../resources/images/' + name + '-' + version + '.png';
  }

  function clearTransientTimers() {
    clearTimeout(holdTimer);
    clearTimeout(resultTimer);
    clearInterval(ringTimer);
    holdTimer = null;
    resultTimer = null;
    ringTimer = null;
  }

  function setScene(waveCount) {
    phoneSprite.style.left = '34px';
    phoneSprite.style.top = '94px';
    phoneSprite.style.width = '203px';
    phoneSprite.style.height = '139px';
    cushionSprite.style.display = waveCount ? 'block' : 'none';
    if (waveCount) cushionSprite.src = image('wave-' + waveCount);
    cushionSprite.style.left = '196px';
    cushionSprite.style.top = '155px';
    cushionSprite.style.width = '84px';
    cushionSprite.style.height = '90px';
  }

  function render(nextState, event, preserveTimers) {
    if (!preserveTimers) clearTransientTimers();
    closedOverlay.style.display = 'none';
    state = nextState;
    var view = states[state];
    title.textContent = view.title;
    device.textContent = view.device || '';
    hint.textContent = view.hint;
    phoneSprite.src = image('scene-foreground');
    setScene(view.wave);
    badge.style.display = view.badge ? 'block' : 'none';
    if (view.badge) badge.src = image('badge-' + view.badge);
    holdTrack.style.display = view.progress ? 'block' : 'none';
    holdProgress.style.transition = 'none';
    holdProgress.style.width = '0';
    document.querySelectorAll('.state-chip').forEach(function(chip) {
      chip.classList.toggle('is-active', chip.dataset.state === state);
    });
    eventLog.textContent = event || ('FORCE_STATE → ' + state);
    if (state === 'RINGING_ACCEPTED') startRingAnimation();
  }

  function startRingAnimation() {
    var phaseIndex = 0;
    var phases = [1, 2, 3, 2];
    ringTimer = setInterval(function() {
      var waveCount = phases[phaseIndex];
      phaseIndex = (phaseIndex + 1) % phases.length;
      cushionSprite.src = image('wave-' + waveCount);
    }, 150);
  }

  function beginHold() {
    if (closedOverlay.style.display === 'grid') {
      render('READY', 'APP_LAUNCH → READY');
      return;
    }
    if (state === 'OFFLINE' || state === 'ERROR') {
      requestSound('RETRY');
      return;
    }
    if (state !== 'READY') return;
    render('ARMING', 'SELECT_DOWN → ARMING');
    requestAnimationFrame(function() {
      holdProgress.style.transition = 'width 650ms linear';
      holdProgress.style.width = '100%';
    });
    holdTimer = setTimeout(function() { requestSound('ARM_TIMEOUT'); }, 650);
  }

  function cancelHold() {
    if (state !== 'ARMING') return;
    clearTimeout(holdTimer);
    render('READY', 'SELECT_UP → READY (cancelled)');
  }

  function requestSound(eventName) {
    render('REQUESTING', eventName + ' → REQUESTING');
    var outcome = document.querySelector('input[name=outcome]:checked').value;
    resultTimer = setTimeout(function() {
      var target = outcome === 'success' ? 'RINGING_ACCEPTED' : outcome.toUpperCase();
      render(target, 'APPLE_RESPONSE → ' + target);
      if (target === 'RINGING_ACCEPTED') {
        resultTimer = setTimeout(function() { render('READY', 'SUCCESS_TIMEOUT → READY'); }, 4000);
      }
    }, 1100);
  }

  Object.keys(states).forEach(function(name) {
    var chip = document.createElement('button');
    chip.type = 'button';
    chip.className = 'state-chip';
    chip.dataset.state = name;
    chip.textContent = name;
    chip.addEventListener('click', function() { render(name); });
    document.getElementById('state-grid').appendChild(chip);
  });

  document.getElementById('select-button').addEventListener('pointerdown', beginHold);
  document.getElementById('select-button').addEventListener('pointerup', cancelHold);
  document.getElementById('select-button').addEventListener('pointerleave', cancelHold);
  document.getElementById('up-button').addEventListener('click', function() {
    if (state !== 'READY') return;
    deviceIndex = (deviceIndex + deviceNames.length - 1) % deviceNames.length;
    device.textContent = deviceNames[deviceIndex];
    states.READY.device = deviceNames[deviceIndex];
    eventLog.textContent = 'UP → SELECT_DEVICE(' + deviceNames[deviceIndex] + ')';
  });
  document.getElementById('down-button').addEventListener('click', function() {
    if (state !== 'READY') return;
    deviceIndex = (deviceIndex + 1) % deviceNames.length;
    device.textContent = deviceNames[deviceIndex];
    states.READY.device = deviceNames[deviceIndex];
    eventLog.textContent = 'DOWN → SELECT_DEVICE(' + deviceNames[deviceIndex] + ')';
  });
  document.getElementById('back-button').addEventListener('click', function() {
    if (state === 'ERROR') render('READY', 'BACK → READY');
    else {
      clearTransientTimers();
      closedOverlay.style.display = 'grid';
      eventLog.textContent = 'BACK → APP_CLOSED';
    }
  });

  document.querySelectorAll('.tab').forEach(function(tab) {
    tab.addEventListener('click', function() {
      document.querySelectorAll('.tab').forEach(function(item) { item.classList.remove('is-active'); });
      document.querySelectorAll('.view').forEach(function(item) { item.classList.remove('is-active'); });
      tab.classList.add('is-active');
      document.getElementById(tab.dataset.view + '-view').classList.add('is-active');
    });
  });

  document.querySelectorAll('[data-next-step]').forEach(function(button) {
    button.addEventListener('click', function() {
      if (button.dataset.nextStep === 'two-factor') document.getElementById('apple-password').value = '';
      document.querySelectorAll('.setup-step').forEach(function(step) { step.classList.remove('is-active'); });
      document.querySelector('[data-setup-step=' + button.dataset.nextStep + ']').classList.add('is-active');
    });
  });

  document.querySelectorAll('.code-inputs input').forEach(function(input, index, all) {
    input.addEventListener('input', function() {
      input.value = input.value.replace(/\D/g, '').slice(0, 1);
      if (input.value && all[index + 1]) all[index + 1].focus();
    });
  });

  render('READY', 'JS_READY → READY');
}());
