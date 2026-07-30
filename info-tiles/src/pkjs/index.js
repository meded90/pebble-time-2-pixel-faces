var WEATHER_CACHE_KEY = 'info_tiles_weather';
var WEATHER_MAX_AGE_MS = 30 * 60 * 1000;

function sendWeather(temperature, weatherCode) {
  Pebble.sendAppMessage({
    TEMPERATURE: temperature,
    WEATHER_CODE: weatherCode
  }, function() {
    console.log('Weather sent to Info Tiles');
  }, function(error) {
    console.log('Weather send failed: ' + JSON.stringify(error));
  });
}

function sendCachedWeather() {
  try {
    var cached = JSON.parse(localStorage.getItem(WEATHER_CACHE_KEY));
    if (cached && Date.now() - cached.savedAt < WEATHER_MAX_AGE_MS) {
      sendWeather(cached.temperature, cached.weatherCode);
      return true;
    }
  } catch (error) {
    console.log('Weather cache unavailable: ' + error);
  }
  return false;
}

function fetchWeather() {
  navigator.geolocation.getCurrentPosition(function(position) {
    var url = 'https://api.open-meteo.com/v1/forecast?' +
      'latitude=' + position.coords.latitude +
      '&longitude=' + position.coords.longitude +
      '&current=temperature_2m,weather_code';

    var request = new XMLHttpRequest();
    request.onload = function() {
      if (request.status < 200 || request.status >= 300) {
        console.log('Open-Meteo HTTP status: ' + request.status);
        return;
      }

      try {
        var result = JSON.parse(request.responseText);
        var temperature = Math.round(result.current.temperature_2m);
        var weatherCode = result.current.weather_code;

        localStorage.setItem(WEATHER_CACHE_KEY, JSON.stringify({
          temperature: temperature,
          weatherCode: weatherCode,
          savedAt: Date.now()
        }));
        sendWeather(temperature, weatherCode);
      } catch (error) {
        console.log('Open-Meteo response error: ' + error);
      }
    };
    request.onerror = function() {
      console.log('Open-Meteo network request failed');
    };
    request.open('GET', url);
    request.send();
  }, function(error) {
    console.log('Location request failed: ' + error.code);
  }, {
    enableHighAccuracy: false,
    maximumAge: WEATHER_MAX_AGE_MS,
    timeout: 15000
  });
}

function updateWeather() {
  if (!sendCachedWeather()) {
    fetchWeather();
  }
}

Pebble.addEventListener('ready', function() {
  updateWeather();
});

Pebble.addEventListener('appmessage', function(event) {
  if (event.payload.REQUEST_WEATHER) {
    fetchWeather();
  }
});

