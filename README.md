# Ultra Bootscreen Manager

Меняй заставку UltraNX прямо на Nintendo Switch: выбирай готовый BMP, смотри предпросмотр и возвращай предыдущую заставку из бэкапа.

**Nintendo Switch homebrew · версия 1.1.1 · RU / EN**

[Скачать NRO 1.1.1](https://github.com/jeronimo701/ultra-bootscreen-manager/releases/tag/v1.1.1) · [Инструкция и управление](DETAILS.ru.md) · [Редактор для Windows](https://github.com/jeronimo701/ultra-bootscreen-editor) · [Проблемы](https://github.com/jeronimo701/ultra-bootscreen-manager/issues)

![Меню выбора заставки](docs/screenshots/menu-ru.png)

*Реальный рендер интерфейса в тестовом окружении на ПК, 1280×720. Это не захват с физической Switch; сервисы консоли имитируются.*

## Возможности

- Свои заставки и три встроенные картинки.
- Полноэкранный предпросмотр перед установкой.
- Первая резервная копия, отдельные бэкапы перед заменой и восстановление.
- Встроенный оригинал заставки UltraNX.
- Переключение RU/EN и перезагрузка только после подтверждения.

## Установка

1. Скопируй `UltraBootscreenManager.nro` из [релиза](https://github.com/jeronimo701/ultra-bootscreen-manager/releases/tag/v1.1.1) в `switch/UltraBootscreenManager/` на SD.
2. Свои BMP положи в подпапку `bootscreens/`.
3. Запусти приложение через hbmenu. Выбери заставку, нажми **X** для предпросмотра, **A** для установки с подтверждением.

```text
switch/UltraBootscreenManager/
├── UltraBootscreenManager.nro
├── bootscreens/       свои BMP
└── backups/           создаётся автоматически
```

Картинки нужны в формате UltraNX: BMP 720×1280, 32 bit, без сжатия, с нужным поворотом. Обычный JPG/PNG сначала подготовь в [Ultra Bootscreen Editor](https://github.com/jeronimo701/ultra-bootscreen-editor).

## Управление

| Кнопка | Действие |
| --- | --- |
| L / R | Заставки / Бэкапы / Инструкция |
| Вверх / вниз | Выбор |
| X | Предпросмотр |
| A | Установить / восстановить, затем подтвердить |
| B | Отмена / назад |
| Y | Русский / English |
| + | Выход в hbmenu |

<details>
<summary>Бэкапы и встроенная справка — ещё скриншоты</summary>

![Бэкапы](docs/screenshots/backups-ru.png)

![Инструкция](docs/screenshots/help-ru.png)

</details>

## Перед использованием

Приложение меняет `sdmc:/bootloader/res/bootscreen.bmp`. Сохрани копию SD на ПК, не выключай консоль и не вынимай карту при записи. Бэкап на той же SD не защищает от повреждения карты.

NRO собран; 22 проверки на ПК описаны в [VALIDATION.md](VALIDATION.md). На физической Switch версия отдельно не проверена. Формат файлов, восстановление после ошибок, сборка и English quick start — в [DETAILS.ru.md](DETAILS.ru.md).

## Авторство и условия

Автор — **Jeronimo701**. [LICENSE.txt](LICENSE.txt) и [LICENSE-SCOPE.md](LICENSE-SCOPE.md): бесплатное личное некоммерческое использование физическими лицами; организациям и для продажи нужно отдельное разрешение. Сторонние компоненты и изображения: [THIRD-PARTY.md](THIRD-PARTY.md). Не официальный продукт Nintendo или UltraNX.
