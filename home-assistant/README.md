# AdaBox (Jukebox) — Home Assistant Integration

Цей модуль забезпечує моніторинг та інтеграцію музичного плеєра **AdaBox** у Home Assistant.

## 📡 Архітектура та мережа

- **Локальний домен:** `http://adabox.lan/` (або `http://adabox.local/`)
- **Фіксований IP:** `192.168.1.88` (налаштовано статичну DHCP-резервацію на UniFi UDR7)
- **JSON API ендпоінт:** `GET http://adabox.lan/info`
- **Веб-панель керування:** `GET http://adabox.lan/`
- **OTA оновлення прошивки:** `GET http://adabox.lan/update`

### Формат відповіді `/info`:
```json
{
  "name": "adabox",
  "version": "2.1.0",
  "status": "idle",
  "mode": "OTHER",
  "track": 1,
  "volume": 12,
  "led": 10,
  "rssi": -71,
  "ip": "192.168.1.88"
}
```

## 🛠️ Підключення сенсорів у `configuration.yaml`

1. Додайте вміст файлу [`adabox_sensors.yaml`](adabox_sensors.yaml) до вашого `configuration.yaml` у секцію `rest:`.
2. Щоб запобігти флуду бази даних SQLite Home Assistant (флуктуація сигналу Wi-Fi RSSI на 1–2 dBm кожні 15 с), обов'язково додайте `sensor.adabox_wi_fi_rssi` до виключень рекордера (`recorder: exclude: entities:`):
   ```yaml
   recorder:
     exclude:
       entities:
         - sensor.adabox_wi_fi_rssi
   ```
   Це дозволяє бачити рівень сигналу наживо в інтерфейсі, але не записувати кожне мікроколивання на диск. `rssi` також навмисно виключено з `json_attributes` сенсора прошивки, тому пристрій у режимі очікування генерує рівно **0** записів у базу даних.
3. Перевірте конфігурацію:
   ```bash
   ha_ctl.py check-config
   ```
4. Перезавантажте REST сутності без рестарту всього HA:
   ```bash
   curl -s -X POST -H "Authorization: Bearer $HA_TOKEN" "$HA_URL/api/services/rest/reload"
   ```

## 📊 Сутності Home Assistant

| Сутність | Тип | Опис |
| :--- | :--- | :--- |
| `binary_sensor.adabox_online` | `connectivity` | Мережева доступність плеєра (`on` / `off` / `unavailable`) |
| `sensor.adabox_firmware_version` | `sensor` | Версія поточної прошивки (напр. `2.1.0`) з усіма JSON атрибутами |
| `sensor.adabox_status` | `sensor` | Поточний стан відтворення (`Playing`, `Paused`, `Idle`) |
| `sensor.adabox_mode` | `sensor` | Поточна папка (`MUSIC` або `OTHER`) |
| `sensor.adabox_track` | `sensor` | Номер треку |
| `sensor.adabox_volume` | `sensor` | Рівень гучності (2–14) |
| `sensor.adabox_led_level` | `sensor` | Рівень яскравості NeoPixel кільця (0–10) |
| `sensor.adabox_wi_fi_rssi` | `signal_strength` | Рівень сигналу Wi-Fi у dBm |

## 🖥️ Дашборд обслуговування (Lovelace)

Вкладка для дашборду `home-maintenance` описана у файлі [`adabox_maintenance_card.yaml`](adabox_maintenance_card.yaml). Вона відображає живий статус усіх параметрів та містить прямі посилання на локальну веб-панель керування та OTA-оновлення прошивки.
