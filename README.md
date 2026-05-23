# Game Backend (C++)

Игровой backend-сервер с REST API, таймерной игровой моделью и сохранением результатов в PostgreSQL.
Проект реализован как законченный сервис: игрок подключается к карте, управляет персонажем, получает очки за игровой процесс, а после периода бездействия автоматически выбывает из сессии с записью результата в таблицу рекордов.

## Возможности

- HTTP API для игрового цикла:
  - вход в игру;
  - получение состояния сессии;
  - управление движением;
  - игровой тик;
  - получение таблицы рекордов.
- Модель карты с дорогами, ограничением перемещения и подбором предметов.
- Начисление очков за игровой прогресс.
- Автоматический retirement игроков по времени неактивности (`dogRetirementTime`).
- Персистентное хранение retired-результатов в PostgreSQL.
- Пагинация таблицы рекордов через `start` и `maxItems`.

## Технологии

- C++20
- Boost (`asio`, `beast`, `json`, `program_options`, `log`)
- PostgreSQL + `libpqxx`
- CMake + Conan
- Docker

## Архитектура

- `src/main.cpp` — точка входа, загрузка конфигурации, запуск HTTP-сервера, инициализация БД.
- `src/application.*` — бизнес-логика игры: игроки, тики, retirement, сценарии использования.
- `src/model.*` — доменная модель: карты, дороги, сессии, персонажи, лут, движение.
- `src/api_handler.h` — HTTP API, валидация запросов и формирование JSON-ответов.
- `src/request_handler.*` / `src/http_server.*` — транспортный слой (Beast/Asio).
- `src/retirement_db.*` / `src/connection_pool.h` — persistence и пул соединений с PostgreSQL.
- `src/serializing_listener.*` — сериализация и восстановление состояния сервера.

## Локальная сборка и запуск

Требования:

- CMake >= 3.11
- Conan 1.x
- компилятор с поддержкой C++20

Сборка:

```bash
mkdir build
cd build
conan install .. --build=missing -s build_type=Release -s compiler.libcxx=libstdc++11
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

Запуск:

```bash
./game_server -c ../data/config.json -w ../static
```

Запуск с PostgreSQL:

```bash
export GAME_DB_URL="postgres://user:password@host:5432/dbname"
./game_server -c ../data/config.json -w ../static
```

## Docker

Сборка:

```bash
docker build -t game-backend .
```

Запуск:

```bash
docker run --rm -p 8080:8080 \
  -e GAME_DB_URL="postgres://user:password@host:5432/dbname" \
  game-backend
```

## Примеры API

- `POST /api/v1/game/join`
- `GET /api/v1/game/state` (Bearer token)
- `POST /api/v1/game/player/action` (Bearer token)
- `POST /api/v1/game/tick`
- `GET /api/v1/game/records?start=0&maxItems=100`

## Итог

Это полноценный backend-движок аркадной игры с прозрачной архитектурой, корректной обработкой игрового времени, безопасной работой с API и персистентной таблицей рекордов.
