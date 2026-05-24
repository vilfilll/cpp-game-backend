# Game Backend (C++)

Игровой backend-сервер на C++20 с REST API, таймерной игровой моделью и хранением результатов в PostgreSQL.

Игрок подключается к игровой карте, управляет персонажем, получает очки за игровой процесс, а после периода бездействия автоматически завершает игровую сессию с сохранением результата в таблицу рекордов.

## Ключевые возможности

### REST API игрового цикла

* подключение игрока;
* получение состояния игры;
* управление движением;
* игровые тики;
* получение таблицы рекордов.

### Игровая модель

* карты и дороги;
* ограничение перемещения;
* генерация предметов;
* начисление очков.

### Работа с данными

* хранение результатов в PostgreSQL;
* пул соединений;
* пагинация таблицы рекордов.

### Надёжность

* автоматическое завершение неактивных игроков (`dogRetirementTime`);
* сериализация и восстановление состояния сервера.

---

## Стек

### Язык

* C++20

### Сетевое взаимодействие

* Boost.Asio
* Boost.Beast
* Boost.JSON
* Boost.Log

### Работа с данными

* PostgreSQL
* libpqxx

### Сборка и инфраструктура

* CMake
* Conan
* Docker

### Тестирование

* Catch2

---

## Архитектура

Проект реализован в виде многослойной архитектуры:

```text
HTTP Request
       ↓
API Layer
(api_handler)

       ↓
Application Layer
(игровые сценарии)

       ↓
Domain Layer
(карты, игроки, лут, игровая логика)

       ↓
Persistence Layer
(PostgreSQL + connection pool)
```

Основные компоненты:

* `application.*` — бизнес-логика игровых сценариев;
* `model.*` — доменная модель: карты, игроки, движение, предметы;
* `api_handler.*` — обработка HTTP API;
* `request_handler.*`, `http_server.*` — транспортный слой;
* `retirement_db.*`, `connection_pool.h` — работа с PostgreSQL;
* `serializing_listener.*` — сериализация и восстановление состояния.

---

## Сборка

### Требования

* CMake >= 3.11
* Conan 1.x
* Компилятор с поддержкой C++20

### Build

```bash
mkdir build
cd build

conan install .. --build=missing \
-s build_type=Release \
-s compiler.libcxx=libstdc++11

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

---

## Запуск

```bash
./game_server -c ../data/config.json -w ../static
```

### Запуск с PostgreSQL

```bash
export GAME_DB_URL="postgres://user:password@host:5432/dbname"

./game_server -c ../data/config.json -w ../static
```

---

## Docker

### Build

```bash
docker build -t game-backend .
```

### Run

```bash
docker run --rm -p 8080:8080 \
-e GAME_DB_URL="postgres://user:password@host:5432/dbname" \
game-backend
```

---

## API

| Method | Endpoint                                    | Description                |
| ------ | ------------------------------------------- | -------------------------- |
| POST   | `/api/v1/game/join`                         | Подключение игрока         |
| GET    | `/api/v1/game/state`                        | Получение состояния игры   |
| POST   | `/api/v1/game/player/action`                | Действия игрока            |
| POST   | `/api/v1/game/tick`                         | Игровой тик                |
| GET    | `/api/v1/game/records?start=0&maxItems=100` | Получение таблицы рекордов |

---

## Особенности реализации

* REST API с Bearer-авторизацией;
* игровая модель с таймерной логикой;
* автоматический retirement игроков;
* хранение результатов в PostgreSQL;
* пул соединений с БД;
* пагинация рекордов;
* сериализация состояния сервера;
* модульная многослойная архитектура.

---
